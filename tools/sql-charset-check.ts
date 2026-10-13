#!/usr/bin/env bun
// sql-charset-check: E2E regression for session-charset restore on reconnect.
// The DB and game servers must already be booted externally by the operator.
// Ownership model: this tool NEVER kills by image name (no taskkill /IM).
// The pre-bounce instances are external and are never terminated by name;
// the bounce uses a port-scoped SHUTDOWN (127.0.0.1:3307, exactly the test
// endpoint) and the replacement instance is tracked by PID and killed only
// via PID-verified taskkill /PID. Game servers are never stopped here: on
// any abort the tool exits and the operator restarts them manually.
// Destructive SQL (bounce + fixture DELETE) runs only against a verified
// disposable instance: --char-db/--login-db must match /^[A-Za-z0-9_]+_test$/,
// the operator must pass --confirm-disposable YES (suffix alone proves
// nothing about the data inside), AND the live server's @@datadir/@@port
// must equal --data-dir/3307 exactly (normalized, never by prefix).
// NOTE: the bounce restarts the WHOLE MariaDB instance on 127.0.0.1:3307 —
// every database it serves, not just the two *_test schemas. Fixture cleanup
// is additionally scoped to
// the login account used by the run, so colliding names on other accounts
// are never touched.
// Flow: control creation with
// non-ASCII bytes on pristine handles (must succeed), bounce (DB restart,
// verified fresh), converge loop (bounded full runs with fresh names;
// handles heal lazily and monotonically, dead->healthy only, so round 1 is
// red by construction and green must be reached quickly). The green round's
// creation must store byte-exact bytes; no name may ever be stored twice.
// Pre-fix the loop never goes green (creation keeps failing on the
// wrong-charset handle), so the same artifact proves both directions.
// Exit 0 on full pass; nonzero otherwise. ABORT lines = harness failure.
//
// Everything environment-specific is a flag; no hosts, paths or secrets
// live here. --db-pass/--user/--pass are mandatory. The servers must run
// with mysql_reconnect_type: 1 and e.g. default_codepage: latin1, on a
// disposable database; never production. -CharDb names the char database
// (default schema table `char`).
//
// Usage:
//   bun tools/sql-charset-check.ts --mariadb-bin C:/path/bin \
//     --data-dir C:/path/data --server-dir C:/path/rathena \
//     --db-user root --db-pass <db-pass> --char-db <test-db> \
//     --login-db <login-db> --user <test-user> --pass <test-pass> \
//     --confirm-disposable YES
import net from "node:net";
import { spawn, execFileSync, type ChildProcess } from "node:child_process";

type Args = Record<string, string>;
const argv: Args = {};
for (let i = 2; i < process.argv.length; i += 2)
	argv[process.argv[i].replace(/^--/, "")] = process.argv[i + 1] ?? "";
const req = ["mariadb-bin", "data-dir", "server-dir", "db-user", "db-pass",
	"char-db", "login-db", "user", "pass", "confirm-disposable"];
for (const k of req) {
	if (!argv[k]) {
		console.log(`ABORT-MISSING-ARG ${k}`);
		process.exit(2);
	}
}
const DB_PORT = 3307, LOGIN_PORT = 6900;
let fails = 0;
function step(name: string, cond: boolean) {
	if (cond) console.log(`PASS ${name}`);
	else { console.log(`FAIL ${name}`); fails++; }
}
const sleep = (ms: number) => new Promise(r => setTimeout(r, ms));
const MARIADB = `${argv["mariadb-bin"]}/mariadb.exe`;
const MARIADBD = `${argv["mariadb-bin"]}/mariadbd.exe`;
function cli(sql: string, db?: string): string {
	const a = ["--host=127.0.0.1", `--port=${DB_PORT}`, `--user=${argv["db-user"]}`,
		`--password=${argv["db-pass"]}`, "--skip-ssl", "-N", "--batch", "-e", sql];
	if (db) a.push(db);
	try {
		return execFileSync(MARIADB, a, { encoding: "utf8" }) as string;
	} catch (e: any) {
		return `CLI-ERROR ${(e as Error).message.split("\n")[0]}`;
	}
}
function waitDb(tag: string): boolean {
	for (let i = 0; i < 30; i++) {
		Bun.sleepSync(3000);
		const r = cli("SELECT 1;");
		if (r.trim() === "1") { console.log(`DB-READY ${tag}`); return true; }
	}
	console.log(`ABORT-NODB ${tag}`); fails++; return false;
}
// Windows-safe directory compare: separators unified, trailing slashes
// dropped (roots like C:\ kept), case folded. Exact equality only — a
// prefix of the expected path never matches.
function normWinDir(p: string): string {
	// Canonicalize separators (including doubled ones the server may
	// report), drop trailing slashes (roots like C:\ kept), fold case.
	let s = p.replace(/\//g, "\\").replace(/\\+/g, "\\");
	while (s.length > 3 && s.endsWith("\\")) s = s.slice(0, -1);
	return s.toUpperCase();
}
// Proves the server behind 127.0.0.1:3307 is exactly the disposable instance
// the operator pointed at. Loopback alone proves nothing: any instance could
// listen there. Unverifiable (CLI-ERROR, empty) counts as mismatch.
function verifyDbIdentity(tag: string): boolean {
	const dd = cli("SELECT @@datadir;").split("\n")[0]?.trim() ?? "";
	const pt = cli("SELECT @@port;").trim();
	const ok = /^\d+$/.test(pt) && parseInt(pt) === DB_PORT
		&& dd !== "" && !dd.startsWith("CLI-ERROR")
		&& normWinDir(dd) === normWinDir(argv["data-dir"] ?? "");
	if (ok) { console.log(`DB-IDENTITY-OK ${tag} port=${pt}`); return true; }
	console.log(`ABORT-DB-IDENTITY-MISMATCH ${tag} port=${pt} datadir=${dd}`);
	return false;
}
class Conn {
	sock: net.Socket;
	buf: Buffer = Buffer.alloc(0);
	ended = false;
	constructor(port: number) {
		this.sock = net.createConnection({ host: "127.0.0.1", port });
		this.sock.on("data", d => { this.buf = Buffer.concat([this.buf, d]); });
		this.sock.on("close", () => { this.ended = true; });
		this.sock.on("error", () => {});
	}
	async ready() {
		// Block until the TCP connection is established (like TcpClient.Connect),
		// instead of racing it: loopback connects can take seconds when the
		// server is sluggish, and sends/receives before that silently stall.
		if (!this.sock.connecting) return;
		await new Promise<void>((resolve) => {
			const done = () => {
				clearTimeout(t);
				this.sock.off("connect", onC);
				this.sock.off("error", onE);
				resolve();
			};
			const t = setTimeout(done, 30000);
			const onC = () => done();
			const onE = () => done();
			this.sock.on("connect", onC);
			this.sock.on("error", onE);
		});
	}
	send(b: Buffer) { this.sock.write(b); }
	async readExact(n: number, ms: number): Promise<Buffer | null> {
		const end = Date.now() + ms;
		while (this.buf.length < n) {
			if (this.ended) return null;
			if (Date.now() > end) return null;
			await sleep(25);
		}
		const out = this.buf.subarray(0, n);
		this.buf = this.buf.subarray(n);
		return Buffer.from(out);
	}
	// Length-framed packet {id,len} or null. Fixed-size packets are NOT parsed here.
	async readPacket(ms: number): Promise<{ id: number, len: number, data: Buffer } | null> {
		const h = await this.readExact(4, ms);
		if (!h) return null;
		const id = h.readUInt16LE(0), len = h.readUInt16LE(2);
		if (len < 4 || len > 32768) return { id, len: -1, data: h };
		const rest = await this.readExact(len - 4, ms);
		if (!rest) return null;
		return { id, len, data: rest };
	}
	async drainCollect(ms: number): Promise<Buffer> {
		const end = Date.now() + ms;
		const parts: Buffer[] = [];
		while (Date.now() < end) {
			if (this.buf.length > 0) {
				parts.push(Buffer.from(this.buf));
				this.buf = Buffer.alloc(0);
			}
			await sleep(100);
		}
		return Buffer.concat(parts);
	}
	close() { try { this.sock.destroy(); } catch {} }
}
const u16 = (v: number) => { const b = Buffer.alloc(2); b.writeUInt16LE(v); return b; };
const u32 = (v: number) => { const b = Buffer.alloc(4); b.writeUInt32LE(v); return b; };
const CLIENT_VER = 20211103;
// Latin-1 byte E9 in every test name (invalid alone in utf8mb4: hard error,
// no silent mojibake). expectedHex: stored utf8mb4 bytes (E9 -> C3A9).
function nameHex(ascii: string): string {
	return Buffer.from(ascii, "ascii").toString("hex").toUpperCase() + "E9";
}
function storedHex(ascii: string): string {
	return Buffer.from(ascii, "ascii").toString("hex").toUpperCase() + "C3A9";
}
function nameField(hex: string): Buffer {
	const raw = Buffer.from(hex, "hex");
	const f = Buffer.alloc(24);
	raw.copy(f, 0, 0, Math.min(raw.length, 24));
	return f;
}
// Only the mariadbd instance spawned by this tool's bounce is ever stopped,
// by PID after verifying the PID still belongs to mariadbd.exe. External
// processes (pre-bounce DB, game servers, anything else on this host) are
// never terminated here.
let ownedDb: ChildProcess | null = null;
function killOwnedDb() {
	if (!ownedDb || ownedDb.pid === undefined) { ownedDb = null; return; }
	try {
		const out = execFileSync("tasklist", ["/FI", `PID eq ${ownedDb.pid}`,
			"/FO", "CSV", "/NH"], { encoding: "utf8" }) as string;
		if (!out.toLowerCase().includes("mariadbd.exe")) {
			console.log(`ABORT-OWNERSHIP-LOST pid=${ownedDb.pid}`);
			ownedDb = null;
			return;
		}
		execFileSync("taskkill", ["/F", "/PID", String(ownedDb.pid)]);
	} catch {}
	ownedDb = null;
}
async function login(): Promise<{ c: Conn, aid: number, l1: number, l2: number, chport: number } | null> {
	const c = new Conn(LOGIN_PORT);
	await c.ready();
	const user = Buffer.alloc(24); Buffer.from(argv["user"]).copy(user);
	const pass = Buffer.alloc(24); Buffer.from(argv["pass"]).copy(pass);
	c.send(Buffer.concat([u16(0x64), u32(CLIENT_VER), user, pass, Buffer.from([0])]));
	const end = Date.now() + 25000;
	let ac = null;
	while (Date.now() < end) {
		const p = await c.readPacket(4000);
		if (!p) break;
		if (p.id === 0xac4 || p.id === 0x83e) { ac = p; break; }
	}
	if (!ac || ac.id !== 0xac4 || ac.len < 224) { c.close(); return null; }
	const s = ac.data;
	return {
		c,
		aid: s.readUInt32LE(4), l1: s.readUInt32LE(0), l2: s.readUInt32LE(8),
		chport: s.readUInt16LE(64),
	};
}
// Create verdict: 0xb6f accept, 0x6e refuse (+errcode), negative = no verdict/timeout.
async function createChar(chport: number, aid: number, l1: number, l2: number,
	hex: string, slot: number): Promise<{ verdict: number, errcode: number }> {
	const c = new Conn(chport);
	await c.ready();
	c.send(Buffer.concat([u16(0x65), u32(aid), u32(l1), u32(l2),
		Buffer.from([0, 0]), Buffer.from([1])]));
	const echo = await c.readExact(4, 5000);
	if (!echo || echo.readUInt32LE(0) !== aid) { c.close(); return { verdict: -2, errcode: -1 }; }
	// Flush stale auth-phase traffic before the verdict window (generous
	// under ASan: the char list can arrive late).
	await c.drainCollect(1500);
	c.send(Buffer.concat([u16(0xa39), nameField(hex), Buffer.from([slot]),
		u16(1), u16(1), u32(0), Buffer.from([1])]));
	const blob = await c.drainCollect(2500);
	c.close();
	if (blob.length < 2) return { verdict: -1, errcode: -1 };
	// Walk packets: a late char list (0x82d, length-framed) may precede the
	// verdict in this window. Skipping it reaches the real verdict; an
	// unknown layout falls back to the first id (previous behavior).
	let o = 0;
	while (o + 2 <= blob.length) {
		const pid = blob.readUInt16LE(o);
		if (pid === 0xb6f) return { verdict: pid, errcode: 0 };
		if (pid === 0x6e) return { verdict: pid, errcode: blob.length > o + 2 ? blob[o + 2] : -1 };
		if (pid === 0x82d && o + 4 <= blob.length) {
			const len = blob.readUInt16LE(o + 2);
			if (len >= 4 && o + len <= blob.length) { o += len; continue; }
		}
		break;
	}
	const id = blob.readUInt16LE(0);
	if (id === 0x6e && blob.length >= 3) return { verdict: id, errcode: blob[2] };
	return { verdict: id, errcode: -1 };
}
// Select slot + map-enter on the SAME session. Returns map bytes (>100 ok).
async function selectMap(chport: number, aid: number, l1: number, l2: number,
	slot: number): Promise<number> {
	const c = new Conn(chport);
	await c.ready();
	c.send(Buffer.concat([u16(0x65), u32(aid), u32(l1), u32(l2),
		Buffer.from([0, 0]), Buffer.from([1])]));
	const echo = await c.readExact(4, 5000);
	if (!echo || echo.readUInt32LE(0) !== aid) { c.close(); return 0; }
	await c.drainCollect(3000);
	c.send(Buffer.concat([u16(0x66), Buffer.from([slot])]));
	const zn = await c.readExact(156, 15000);
	if (!zn || zn.readUInt16LE(0) !== 0xac5) { c.close(); return 0; }
	const cid = zn.readUInt32LE(2);
	const mport = zn.readUInt16LE(26);
	console.log(`map: CID=${cid} srv=127.0.0.1:${mport}`);
	c.close();
	const m = new Conn(mport);
	await m.ready();
	m.send(Buffer.concat([u16(0x0888), u32(aid), u32(cid), u32(l1),
		u32(0), Buffer.from([1])]));
	const data = await m.drainCollect(8000);
	m.close();
	console.log(`c bytes from map: ${data.length}`);
	return data.length;
}
async function main() {
	// Destructive ops (bounce + fixture DELETE) need two independent proofs:
	// a fully-allowlisted identifier AND an explicit operator assertion that
	// the target is disposable. Either check runs before anything is touched
	// and never stops external processes.
	for (const k of ["char-db", "login-db"]) {
		if (!/^[A-Za-z0-9_]+_test$/.test(argv[k] ?? "")) {
			console.log(`ABORT-UNSAFE-TARGET ${k}=${argv[k] ?? ""}`);
			process.exit(2);
		}
	}
	if (!/^[A-Za-z0-9_]+$/.test(argv["user"] ?? "")) {
		console.log(`ABORT-UNSAFE-USER user=${argv["user"] ?? ""}`);
		process.exit(2);
	}
	if (argv["confirm-disposable"] !== "YES") {
		console.log("ABORT-NOT-CONFIRMED-DISPOSABLE");
		process.exit(2);
	}
	// --db-pass/--pass are never interpolated into SQL (protocol only).
	if (!waitDb("up")) { console.log("OPERATOR-ACTION restart test DB + servers manually"); console.log(`FAILURES=${fails}`); process.exit(fails); }
	// Identity before ANY destructive op: a wrong --data-dir (or somebody
	// else's instance on this port) aborts here, SHUTDOWN never sent.
	if (!verifyDbIdentity("pre")) { console.log("OPERATOR-ACTION check --data-dir and test instance"); process.exit(2); }
	// Fixture account must exist (created once by the operator procedure);
	// a missing fixture is a harness failure, not a server verdict.
	const fxid = cli(`SELECT account_id FROM \`${argv["login-db"]}\`.\`login\` WHERE userid='${argv["user"]}';`);
	if (!/^\d+$/.test(fxid.trim())) {
		console.log(`ABORT-NO-FIXTURE-ACCOUNT ${argv["user"]}`);
		console.log("OPERATOR-ACTION restart test servers manually if needed");
		process.exit(2);
	}
	console.log(`FIXTURE-OK ${argv["user"]} aid=${fxid.trim()}`);
	// Control on pristine handles: non-ASCII creation must succeed.
	const L = await login();
	step("1-login-accept", L !== null);
	if (!L) { console.log("OPERATOR-ACTION restart test servers manually if needed"); console.log(`FAILURES=${fails}`); process.exit(fails); }
	console.log(`login: AID=${L.aid} char=127.0.0.1:${L.chport}`);
	// Owner scope for every later fixture query: creation and cleanup touch
	// only this account, so identical names on other accounts are never
	// counted or deleted. AID comes from the server packet (uint32).
	const ownerAid = L.aid >>> 0;
	const ctl = await createChar(L.chport, L.aid, L.l1, L.l2, nameHex("LatA"), 0);
	step("2-control-create", ctl.verdict === 0xb6f);
	L.c.close();
	// Bounce: port-scoped SHUTDOWN reaches exactly the verified server on
	// 127.0.0.1:3307 (never by image name), then boot our own instance and
	// track it. SCOPE: this restarts the whole MariaDB instance — every
	// database it serves, not just the *_test schemas. External game servers
	// are left alone throughout.
	console.log("BOUNCE-SHUTDOWN port=3307 scope=whole-instance");
	cli("SHUTDOWN;");
	await sleep(8000);
	ownedDb = spawn(MARIADBD, [`--datadir=${argv["data-dir"]}`, "--bind-address=127.0.0.1",
		`--port=${DB_PORT}`], { cwd: argv["mariadb-bin"], stdio: "ignore", detached: true });
	try { ownedDb.unref(); } catch {}
	if (!waitDb("post")) { killOwnedDb(); console.log(`FAILURES=${fails}`); process.exit(fails); }
	// The replacement must be our instance too, or the final DELETE stays off.
	if (!verifyDbIdentity("post")) { killOwnedDb(); process.exit(2); }
	const up = cli("SHOW GLOBAL STATUS LIKE 'Uptime';").trim().split(/[\t ]+/).pop() ?? "?";
	if (/^\d+$/.test(up) && parseInt(up) < 180) console.log(`BOUNCE-OK uptime=${up}`);
	else { console.log(`ABORT-BOUNCE-FAILED uptime=${up}`); fails++; }
	// Converge: bounded full runs with fresh names. Round 1 is red by
	// construction (nothing heals the ipban gate before it); green must be
	// reached quickly as handles heal one dead handle per failed round.
	let greenRound = -1;
	let round1red = false;
	for (let i = 1; i <= 6; i++) {
		const ascii = `LatR${i}`;
		const Li = await login();
		if (!Li) {
			console.log(`round${i}-login-red`);
			if (i === 1) round1red = true;
			continue;
		}
		const cr = await createChar(Li.chport, Li.aid, Li.l1, Li.l2, nameHex(ascii), i);
		if (cr.verdict !== 0xb6f) {
			console.log(`round${i}-create-red verdict=0x${cr.verdict.toString(16)}`);
			Li.c.close();
			if (i === 1) round1red = true;
			continue;
		}
		const bytes = await selectMap(Li.chport, Li.aid, Li.l1, Li.l2, i);
		Li.c.close();
		if (bytes <= 100) {
			console.log(`round${i}-map-red`);
			if (i === 1) round1red = true;
			continue;
		}
		console.log(`round${i}-green`);
		greenRound = i;
		break;
	}
	step("3-round1-red", round1red);
	step("4-green-reached", greenRound > 0);
	// Byte-exact + no-duplicate oracle on the green round (if any),
	// both scoped to the run's owner account.
	if (greenRound > 0) {
		const want = storedHex(`LatR${greenRound}`);
		const got = cli(`SELECT HEX(name) FROM \`${argv["char-db"]}\`.\`char\` WHERE account_id=${ownerAid} AND HEX(name)='${want}';`);
		step("5-bytes-exact", got.includes(want));
	}
	const hexes = ["4C617441C3A9", "4C61745231C3A9", "4C61745232C3A9",
		"4C61745233C3A9", "4C61745234C3A9", "4C61745235C3A9", "4C61745236C3A9"];
	const cnt = cli(`SELECT COUNT(*) FROM \`${argv["char-db"]}\`.\`char\` WHERE account_id=${ownerAid} AND HEX(name) IN (${hexes.map(h => `'${h}'`).join(",")});`);
	step("6-no-duplicates", /^\d+$/.test(cnt.trim()) && parseInt(cnt.trim()) <= 2);
	cli(`DELETE FROM \`${argv["char-db"]}\`.\`char\` WHERE account_id=${ownerAid} AND HEX(name) IN (${hexes.map(h => `'${h}'`).join(",")});`);
	console.log("CLEANUP-DONE");
	killOwnedDb();
	console.log("OPERATOR-ACTION test servers left running; stop them manually");
	console.log(`FAILURES=${fails}`);
	process.exit(fails);
}

main();
