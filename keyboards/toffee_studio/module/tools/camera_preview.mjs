// One native camera owner, shared by every local viewer and snapshot request.
import {spawn} from 'node:child_process';
import {readFileSync} from 'node:fs';
import {createServer} from 'node:http';
import {createInterface} from 'node:readline';
import {parseArgs} from 'node:util';

const {values} = parseArgs({options: {
  'camera-tool': {type: 'string'}, device: {type: 'string'}, port: {type: 'string'},
}});
const port = Number(values.port);
if (!values['camera-tool'] || !values.device || !Number.isInteger(port) || port < 1024 || port > 65535) {
  throw new Error('Required: --camera-tool PATH --device DEVICE_ID --port PORT (1024–65535).');
}
const page = readFileSync(new URL('./camera_preview.html', import.meta.url));
const clients = new Set();
const state = {ready: false, alive: false, camera: '', helperPid: null,
  sessionStarts: 0, sessionStops: 0, interruptions: 0, frame: null, error: null};
let camera;
let pending;
let lastFrame;
let streaming = false;
let closing = false;
const delay = ms => new Promise(resolve => setTimeout(resolve, ms));

function fail(message) {
  state.error = message;
  state.ready = false;
  if (pending) {
    clearTimeout(pending.timer);
    pending.reject(new Error(message));
    pending = undefined;
  }
  for (const response of clients) response.end();
  clients.clear();
  console.error(message);
}

function frame() {
  if (!state.ready) return Promise.reject(new Error(state.error || 'Camera is starting.'));
  if (pending) return pending.promise;
  let resolve, reject;
  const promise = new Promise((yes, no) => { resolve = yes; reject = no; });
  pending = {promise, resolve, reject, timer: setTimeout(() => fail('Camera frame timed out.'), 6000)};
  camera.stdin.write('frame\n');
  return promise;
}

async function stream() {
  if (streaming) return;
  streaming = true;
  try {
    while (clients.size && !closing) {
      if (!state.ready) {
        if (state.error) throw new Error(state.error);
        await delay(200);
        continue;
      }
      const image = await frame();
      for (const response of clients) {
        // A slow/hidden viewer cannot accumulate an unbounded video queue.
        if (response.writableLength > 1024 * 1024) continue;
        response.write(`--frame\r\nContent-Type: image/jpeg\r\nContent-Length: ${image.jpeg.length}\r\nX-Frame-Number: ${image.frame}\r\n\r\n`);
        response.write(image.jpeg);
        response.write('\r\n');
      }
      await delay(200);
    }
  } catch (error) {
    fail(error.message);
  } finally {
    streaming = false;
  }
}

const server = createServer(async (request, response) => {
  response.setHeader('Cache-Control', 'no-store');
  response.setHeader('X-Content-Type-Options', 'nosniff');
  response.setHeader('X-Frame-Options', 'DENY');
  const hosts = [`127.0.0.1:${port}`, `localhost:${port}`];
  if (!hosts.includes(request.headers.host) || request.headers['sec-fetch-site'] === 'cross-site') {
    response.writeHead(403).end('Local preview only.');
    return;
  }
  if (request.method !== 'GET') { response.writeHead(405).end(); return; }
  const path = new URL(request.url, `http://127.0.0.1:${port}`).pathname;
  if (path === '/') {
    response.writeHead(200, {'Content-Type': 'text/html; charset=utf-8'}).end(page);
  } else if (path === '/status') {
    response.writeHead(200, {'Content-Type': 'application/json'}).end(JSON.stringify({
      ...state, viewers: clients.size,
      frameAgeMs: lastFrame ? Date.now() - lastFrame.received : null,
    }));
  } else if (path === '/stream') {
    if (state.error) { response.writeHead(503).end(state.error); return; }
    response.writeHead(200, {'Content-Type': 'multipart/x-mixed-replace; boundary=frame'});
    response.flushHeaders();
    clients.add(response);
    response.on('close', () => clients.delete(response));
    void stream();
  } else if (path === '/frame.jpg') {
    try {
      const image = await frame();
      response.writeHead(200, {'Content-Type': 'image/jpeg', 'X-Frame-Number': image.frame,
        'X-Camera-Pid': state.helperPid}).end(image.jpeg);
    } catch (error) { response.writeHead(503).end(error.message); }
  } else { response.writeHead(404).end('Not found.'); }
});

function shutdown() {
  if (closing) return;
  closing = true;
  for (const response of clients) response.end();
  clients.clear();
  server.close();
  if (camera && state.alive) {
    camera.stdin.end('quit\n');
    setTimeout(() => { if (state.alive) camera.kill('SIGTERM'); }, 2500).unref();
  }
}
process.on('SIGINT', shutdown);
process.on('SIGTERM', shutdown);
server.on('error', error => { fail(error.message); shutdown(); process.exitCode = 1; });

// Bind first: a conflicting port must not activate another camera session.
server.listen(port, '127.0.0.1', () => {
  console.log(`Camera preview: http://127.0.0.1:${port}`);
  camera = spawn(values['camera-tool'], ['--serve', values.device], {stdio: ['pipe', 'pipe', 'pipe']});
  state.alive = true;
  camera.on('error', error => fail(error.message));
  camera.stdin.on('error', error => fail(error.message));
  camera.stderr.on('data', data => process.stderr.write(data));
  camera.on('exit', (code, signal) => {
    state.alive = false;
    fail(`Camera exited (${signal || code}).`);
    if (closing) process.exit(code || 0);
  });
  createInterface({input: camera.stdout}).on('line', line => {
    const event = JSON.parse(line);
    if (event.event === 'frame') {
      const image = {...event, jpeg: Buffer.from(event.jpeg_base64, 'base64'), received: Date.now()};
      delete image.jpeg_base64;
      lastFrame = image;
      state.frame = event.frame;
      if (pending) {
        clearTimeout(pending.timer);
        pending.resolve(image);
        pending = undefined;
      }
      return;
    }
    console.log(JSON.stringify(event));
    state.helperPid = event.pid;
    if (event.event === 'session_started') state.sessionStarts += 1;
    if (event.event === 'session_stopped') state.sessionStops += 1;
    if (event.event === 'session_interrupted') state.interruptions += 1;
    if (event.event === 'ready') { state.ready = true; state.camera = event.camera; }
    if (event.event === 'session_error' || event.event === 'request_error') fail(event.detail || event.message);
  });
});
