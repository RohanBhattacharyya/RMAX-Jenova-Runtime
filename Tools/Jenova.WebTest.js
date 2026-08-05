/*
 * Runs an exported Godot Web build in headless Chromium and prints what it logs.
 *
 * Chromium's headless mode has no way to hand back console output on the command line, so
 * this attaches over the DevTools protocol and forwards console messages and page errors
 * until the page logs the marker it is told to wait for.
 *
 *   node Jenova.WebTest.js <url> [marker] [timeout-seconds]
 */

const { spawn } = require("child_process");
const http = require("http");

const url = process.argv[2];
const marker = process.argv[3] || "JENOVA_WEB_TEST";
const timeoutSeconds = Number(process.argv[4] || 120);

if (!url) {
	console.error("usage: node Jenova.WebTest.js <url> [marker] [timeout-seconds]");
	process.exit(2);
}

const port = 9222 + (process.pid % 500);
const chromium = spawn("chromium", [
	"--headless=new",
	"--disable-gpu",
	"--no-sandbox",
	"--use-gl=swiftshader",
	"--enable-unsafe-swiftshader",
	`--remote-debugging-port=${port}`,
	"--user-data-dir=/tmp/claude-1000/chromium-jenova",
	"about:blank",
], { stdio: ["ignore", "ignore", "pipe"] });

let finished = false;
function done(code, message) {
	if (finished) return;
	finished = true;
	if (message) console.log(message);
	try { chromium.kill("SIGKILL"); } catch (_) {}
	process.exit(code);
}

const failTimer = setTimeout(() => done(1, `TIMEOUT: '${marker}' not seen in ${timeoutSeconds}s`), timeoutSeconds * 1000);
failTimer.unref?.();

function fetchTargets(attempt = 0) {
	http.get({ host: "127.0.0.1", port, path: "/json/list" }, (res) => {
		let body = "";
		res.on("data", (chunk) => (body += chunk));
		res.on("end", () => attach(JSON.parse(body)));
	}).on("error", () => {
		if (attempt > 60) return done(1, "Chromium did not expose a DevTools endpoint.");
		setTimeout(() => fetchTargets(attempt + 1), 250);
	});
}

function attach(targets) {
	const page = targets.find((t) => t.type === "page");
	if (!page) return done(1, "No page target.");

	const socket = new WebSocket(page.webSocketDebuggerUrl);
	let nextId = 1;
	const send = (method, params) => socket.send(JSON.stringify({ id: nextId++, method, params }));

	socket.onopen = () => {
		send("Runtime.enable");
		send("Log.enable");
		send("Page.enable");
		send("Page.navigate", { url });
	};

	socket.onmessage = (event) => {
		const message = JSON.parse(event.data);
		let text = null;
		if (message.method === "Runtime.consoleAPICalled") {
			text = message.params.args.map((a) => a.value ?? a.description ?? "").join(" ");
		} else if (message.method === "Log.entryAdded") {
			text = message.params.entry.text;
		} else if (message.method === "Runtime.exceptionThrown") {
			text = "EXCEPTION: " + (message.params.exceptionDetails.exception?.description || message.params.exceptionDetails.text);
		}
		if (text === null || text === "") return;
		console.log(text);
		if (text.includes(marker)) done(text.includes("PASS") ? 0 : 1);
	};

	socket.onerror = () => done(1, "DevTools socket error.");
}

fetchTargets();
