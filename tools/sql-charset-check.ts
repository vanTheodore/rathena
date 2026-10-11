#!/usr/bin/env bun
// sql-charset-check: E2E regression for session-charset restore on reconnect.
// The DB must already run and the game servers must already be booted
// externally (this tool never starts services). Flow: control creation with
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
//     --user <test-user> --pass <test-pass>
import net from "node:net";
import { spawn, execFileSync } from "node:child_process";

type Args = Record<string, string>;
const argv: Args = {};
for (let i = 2; i < process.argv.length; i += 2)
	argv[process.argv[i].replace(/^--/, "")] = process.argv[i + 1] ?? "";
const req = ["mariadb-bin", "data-dir", "server-dir", "db-user", "db-pass",
	"char-db", "user", "pass"];
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
function stopAll() {
	try { execFileSync("taskkill", ["/F", "/IM", "login-server.exe"]); } catch {}
	try { execFileSync("taskkill", ["/F", "/IM", "char-server.exe"]); } catch {}
	try { execFileSync("taskkill", ["/F", "/IM", "map-server.exe"]); } catch {}
	try { execFileSync("taskkill", ["/F", "/IM", "mariadbd.exe"]); } catch {}
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
	// Flush stale auth-phase traffic before the verdict window.
	await c.drainCollect(800);
	c.send(Buffer.concat([u16(0xa39), nameField(hex), Buffer.from([slot]),
		u16(1), u16(1), u32(0), Buffer.from([1])]));
	const blob = await c.drainCollect(2500);
	c.close();
	if (blob.length < 2) return { verdict: -1, errcode: -1 };
	const id = blob.readUInt16LE(0);
	if (id === 0xb6f) return { verdict: id, errcode: 0 };
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
	if (!waitDb("up")) { stopAll(); console.log(`FAILURES=${fails}`); process.exit(fails); }
	// Control on pristine handles: non-ASCII creation must succeed.
	const L = await login();
	step("1-login-accept", L !== null);
	if (!L) { stopAll(); console.log(`FAILURES=${fails}`); process.exit(fails); }
	console.log(`login: AID=${L.aid} char=127.0.0.1:${L.chport}`);
	const ctl = await createChar(L.chport, L.aid, L.l1, L.l2, nameHex("LatA"), 0);
	step("2-control-create", ctl.verdict === 0xb6f);
	L.c.close();
	// Bounce: restart the DB (kills every handle, deterministic).
	try { execFileSync("taskkill", ["/F", "/IM", "mariadbd.exe"]); } catch {}
	await sleep(8000);
	spawn(MARIADBD, [`--datadir=${argv["data-dir"]}`, "--bind-address=127.0.0.1",
		`--port=${DB_PORT}`], { cwd: argv["mariadb-bin"], stdio: "ignore", detached: true }).unref();
	if (!waitDb("post")) { stopAll(); console.log(`FAILURES=${fails}`); process.exit(fails); }
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
	// Byte-exact + no-duplicate oracle on the green round (if any).
	if (greenRound > 0) {
		const want = storedHex(`LatR${greenRound}`);
		const got = cli(`SELECT HEX(name) FROM ${argv["char-db"]}.\`char\` WHERE HEX(name)='${want}';`);
		step("5-bytes-exact", got.includes(want));
	}
	const hexes = ["4C617441C3A9", "4C61745231C3A9", "4C61745232C3A9",
		"4C61745233C3A9", "4C61745234C3A9", "4C61745235C3A9", "4C61745236C3A9"];
	const cnt = cli(`SELECT COUNT(*) FROM ${argv["char-db"]}.\`char\` WHERE HEX(name) IN (${hexes.map(h => `'${h}'`).join(",")});`);
	step("6-no-duplicates", /^\d+$/.test(cnt.trim()) && parseInt(cnt.trim()) <= 2);
	cli(`DELETE FROM ${argv["char-db"]}.\`char\` WHERE HEX(name) IN (${hexes.map(h => `'${h}'`).join(",")});`);
	console.log("CLEANUP-DONE");
	stopAll();
	console.log(`FAILURES=${fails}`);
	process.exit(fails);
}

main();
