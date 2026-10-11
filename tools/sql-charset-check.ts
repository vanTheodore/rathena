#!/usr/bin/env bun
// sql-charset-check: E2E regression for session-charset restore on reconnect.
// Boots the servers (configured with a non-default codepage + type 1),
// creates a non-ASCII char (control, proves the session charset works),
// bounces the database, attempts another creation (heal: must fail clean,
// proving first-touch fail-closed), then creates again (expose: must succeed
// with byte-exact storage once the charset is restored; pre-fix it fails
// with a server 1366 error because the fresh connection speaks utf8mb4).
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
//     --user <test-user> --pass <test-pass> --char <fixture-account-fresh-char-unused>
import net from "node:net";
import { spawn, execFileSync } from "node:child_process";
import fs from "node:fs";

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
const DB_PORT = 3307, LOGIN_PORT = 6900, BOOT_WAIT = 75000;
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
		for (let i = 0; i < 100 && !this.sock.connecting; i++) await sleep(10);
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
let procs: any[] = [];
function startExe(exe: string, log: string) {
	const out = fs.openSync(`${argv["server-dir"]}/${log}.out.log`, "w");
	const err = fs.openSync(`${argv["server-dir"]}/${log}.err.log`, "w");
	const p = spawn(`${argv["server-dir"]}/${exe}`, [],
		{ cwd: argv["server-dir"], stdio: ["ignore", out, err], detached: true });
	(p as any).unref?.();
	procs.push(p);
	return p;
}
function stopAll() {
	for (const p of procs) { try { p.kill("SIGKILL"); } catch {} }
	procs = [];
	try { execFileSync("taskkill", ["/F", "/IM", "login-server.exe"]); } catch {}
	try { execFileSync("taskkill", ["/F", "/IM", "char-server.exe"]); } catch {}
	try { execFileSync("taskkill", ["/F", "/IM", "map-server.exe"]); } catch {}
	try { execFileSync("taskkill", ["/F", "/IM", "mariadbd.exe"]); } catch {}
}
// nameHex: hex bytes of the raw name (e.g. latin1 4C617441E9), NUL-padded to 24.
function nameField(hex: string): Buffer {
	const raw = Buffer.from(hex, "hex");
	const f = Buffer.alloc(24);
	raw.copy(f, 0, 0, Math.min(raw.length, 24));
	return f;
}
async function login(): Promise<{ c: Conn, s: any, aid: number, l1: number, l2: number, chport: number } | null> {
	const c = new Conn(LOGIN_PORT);
	await c.ready();
	const user = Buffer.alloc(24); Buffer.from(argv["user"]).copy(user);
	const pass = Buffer.alloc(24); Buffer.from(argv["pass"]).copy(pass);
	c.send(Buffer.concat([u16(0x64), u32(CLIENT_VER), user, pass, Buffer.from([0])]));
	const end = Date.now() + 10000;
	let ac = null;
	while (Date.now() < end) {
		const p = await c.readPacket(1500);
		if (!p) break;
		if (p.id === 0xac4 || p.id === 0x83e) { ac = p; break; }
	}
	if (!ac || ac.id !== 0xac4 || ac.len < 224) { c.close(); return null; }
	const s = ac.data;
	return {
		c, s,
		aid: s.readUInt32LE(4), l1: s.readUInt32LE(0), l2: s.readUInt32LE(8),
		chport: s.readUInt16LE(64),
	};
}
// Returns the verdict packet id (0xb6f accept, 0x6e refuse) or -1 on timeout/other.
async function createChar(chport: number, aid: number, l1: number, l2: number,
	nameHex: string, slot: number): Promise<{ verdict: number, errcode: number }> {
	const c = new Conn(chport);
	await c.ready();
	const s = c as any;
	c.send(Buffer.concat([u16(0x65), u32(aid), u32(l1), u32(l2),
		Buffer.from([0, 0]), Buffer.from([1])]));
	const echo = await c.readExact(4, 5000);
	if (!echo || echo.readUInt32LE(0) !== aid) { c.close(); return { verdict: -2, errcode: -1 }; }
	// Flush stale auth-phase traffic (pincode etc.) before the verdict window.
	await c.drainCollect(800);
	c.send(Buffer.concat([u16(0xa39), nameField(nameHex), Buffer.from([slot]),
		u16(1), u16(1), u32(0), Buffer.from([1])]));
	const blob = await c.drainCollect(2500);
	c.close();
	if (blob.length < 2) return { verdict: -1, errcode: -1 };
	const id = blob.readUInt16LE(0);
	if (id === 0xb6f) return { verdict: id, errcode: 0 };
	if (id === 0x6e && blob.length >= 3) return { verdict: id, errcode: blob[2] };
	return { verdict: id, errcode: -1 };
}
async function main() {
	// The DB must already run (game servers are booted externally and stay
	// up across the bounce below); only the bounce itself restarts it.
	if (!waitDb("up")) { stopAll(); console.log(`FAILURES=${fails}`); process.exit(fails); }
	// Boot servers.
	startExe("login-server.exe", "t-login");
	await sleep(8000);
	startExe("char-server.exe", "t-char");
	await sleep(12000);
	startExe("map-server.exe", "t-map");
	await sleep(BOOT_WAIT);
	// Control: login + create latin1 name in slot 0.
	const L = await login();
	step("1-login-accept", L !== null);
	if (!L) { stopAll(); console.log(`FAILURES=${fails}`); process.exit(fails); }
	console.log(`login: AID=${L.aid} char=127.0.0.1:${L.chport}`);
	const ctl = await createChar(L.chport, L.aid, L.l1, L.l2, "4C617441E9", 0);
	step("2-control-create", ctl.verdict === 0xb6f);
	L.c.close();
	// Select + map-enter the control char to prove usability + save path.
	{
		const c = new Conn(L.chport);
		await c.ready();
		c.send(Buffer.concat([u16(0x65), u32(L.aid), u32(L.l1), u32(L.l2),
			Buffer.from([0, 0]), Buffer.from([1])]));
		await c.readExact(4, 5000);
		await c.drainCollect(3000);
		c.send(Buffer.concat([u16(0x66), Buffer.from([0])]));
		const zn = await c.readExact(156, 15000);
		let ok = false;
		if (zn && zn.readUInt16LE(0) === 0xac5) {
			const cid = zn.readUInt32LE(2);
			const mip = `${zn[22]}.${zn[23]}.${zn[24]}.${zn[25]}`;
			const mport = zn.readUInt16LE(26);
			console.log(`map: CID=${cid} srv=${mip}:${mport}`);
			c.close();
			const m = new Conn(mport);
			await m.ready();
			m.send(Buffer.concat([u16(0x0888), u32(L.aid), u32(cid), u32(L.l1),
				u32(0), Buffer.from([1])]));
			const data = await m.drainCollect(8000);
			m.close();
			console.log(`c bytes from map: ${data.length}`);
			ok = data.length > 100;
		} else c.close();
		step("3-control-map", ok);
	}
	// Bounce: restart the DB (kills every handle, deterministic).
	try { execFileSync("taskkill", ["/F", "/IM", "mariadbd.exe"]); } catch {}
	await sleep(8000);
	spawn(MARIADBD, [`--datadir=${argv["data-dir"]}`, "--bind-address=127.0.0.1",
		`--port=${DB_PORT}`], { cwd: argv["mariadb-bin"], stdio: "ignore", detached: true }).unref();
	if (!waitDb("post")) { stopAll(); console.log(`FAILURES=${fails}`); process.exit(fails); }
	const up = cli("SHOW GLOBAL STATUS LIKE 'Uptime';").trim().split(/[\t ]+/).pop() ?? "?";
	if (/^\d+$/.test(up) && parseInt(up) < 180) console.log(`BOUNCE-OK uptime=${up}`);
	else { console.log(`ABORT-BOUNCE-FAILED uptime=${up}`); fails++; }
	// Heal: creation on the dead handle must fail clean (red), never retry.
	const L2 = await login();
	step("4-login-accept", L2 !== null);
	if (!L2) { stopAll(); console.log(`FAILURES=${fails}`); process.exit(fails); }
	const heal = await createChar(L2.chport, L2.aid, L2.l1, L2.l2, "4865616C58", 0);
	const healRed = heal.verdict !== 0xb6f;
	console.log(`heal-verdict=0x${heal.verdict.toString(16)} (expected non-0xb6f)`);
	if (!healRed) { console.log("UNEXPECTED-GREEN-HEAL"); fails++; }
	else step("5-heal-fails-clean", true);
	L2.c.close();
	// Expose: creation on the re-established handle. Post-fix this succeeds
	// with byte-exact storage; pre-fix the server rejects it (1366).
	const L3 = await login();
	if (!L3) { console.log("FAIL 6-expose-login"); fails++; }
	else {
		const ex = await createChar(L3.chport, L3.aid, L3.l1, L3.l2, "4C617442E9", 1);
		step("6-expose-create", ex.verdict === 0xb6f);
		if (ex.verdict === 0x6e) console.log(`expose-refused error=0x${ex.errcode.toString(16)}`);
		L3.c.close();
	}
	// Byte-exact storage check (stored utf8mb4 bytes of latin1 E9).
	const hex = cli(`SELECT HEX(name) FROM ${argv["char-db"]}.\`char\` WHERE HEX(name)='4C617442C3A9';`);
	step("7-bytes-exact", hex.includes("4C617442C3A9"));
	// Counts: each latin1 name at most once (no duplicate execution), and the
	// heal name must be absent (a green heal would be a retry/re-execution).
	const cnt = cli(`SELECT COUNT(*) FROM ${argv["char-db"]}.\`char\` WHERE HEX(name) IN ('4C617441C3A9','4C617442C3A9');`);
	const cntHeal = cli(`SELECT COUNT(*) FROM ${argv["char-db"]}.\`char\` WHERE HEX(name)='4865616C58';`);
	step("8-no-duplicates", cnt.trim() === "2" && cntHeal.trim() === "0");
	// Tidy test rows.
	cli(`DELETE FROM ${argv["char-db"]}.\`char\` WHERE HEX(name) IN ('4C617441C3A9','4C617442C3A9','4865616C58');`);
	console.log("CLEANUP-DONE");
	stopAll();
	console.log(`FAILURES=${fails}`);
	process.exit(fails);
}

main();
