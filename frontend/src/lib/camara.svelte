<script lang="ts">
  import { onMount, onDestroy } from 'svelte';
  import { browser } from '$app/environment';

  // --- ESTADOS OPTIMIZADOS (Svelte 5) ---
  let videoElement = $state<HTMLVideoElement | null>(null);
  let status = $state("SYSTEM_BOOTING...");
  let authorizedUser = $state<any>(null);
  let isProcessing = false; 
  let blinkCount = $state(0);
  let isBlinking = false;
  
  let precision = $state("0.0000000");
  let facePos = $state({ x: 0, y: 0, w: 0, h: 0 });
  let userThumb = $state<string | null>(null);

  // --- TERMINAL DE LOGS ---
  let logs = $state<{msg: string, time: string, type: 'info' | 'warn' | 'success'}[]>([]);

  function addLog(msg: string, type: 'info' | 'warn' | 'success' = 'info') {
    const time = new Date().toLocaleTimeString('en-GB', { hour12: false });
    logs = [{ msg, time, type }, ...logs].slice(0, 8);
  }

  // --- MOTOR WASM Y RENDIMIENTO ---
  let engine: any;
  let wasmModule: any;
  let sharedBufferPtr: number;
  const DIMS = 478 * 3;
  let videoWidth = 0;
  let videoHeight = 0;

  let canvasThumb: HTMLCanvasElement;
  let ctxThumb: CanvasRenderingContext2D | null;

  // --- REGISTRO ---
  let isRegistering = $state(false);
  let rawPoseSamples: Float32Array[] = [];
  let anglesDone = $state({ left: false, right: false, up: false, down: false });

  // --- INTEGRACIÓN BACKEND (SQLite Vault) ---
  async function fetchBiometricDB() {
    try {
      const res = await fetch('/api/get-biometrics');
      if (!res.ok) throw new Error();
      const data = await res.json();
      
      return data.map((u: any) => {
        // Mapeo: Maneja 'value_data' de SQLite o 'model' de la estructura JSON
        const rawContent = u.value_data || u.model;
        const parsed = typeof rawContent === 'string' ? JSON.parse(rawContent) : rawContent;
        
        return {
          id: u.id || parsed.id,
          key_name: u.key_name,
          thumb: parsed.thumb,
          model: parsed.model || parsed 
        };
      });
    } catch (e) {
      addLog("DB_FETCH_FAILED", "warn");
      return [];
    }
  }

  async function syncProfileToDB(userData: any) {
    try {
      const res = await fetch('/api/save-biometrics', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ 
          key: `BIO_${userData.id}`, 
          value: JSON.stringify(userData) 
        })
      });
      return res.ok;
    } catch (e) {
      addLog("SERVER_SYNC_FAILED", "warn");
      return false;
    }
  }

  async function loadWasmEngine() {
    const scriptPath = '/main.js'; 
    const wasmGlue = await import(/* @vite-ignore */ scriptPath);
    return await wasmGlue.default({ 
      locateFile: (path: string) => {
        if (path.endsWith('.wasm')) return `/${path}`;
        return `/${path}`;
      }
    });
  }

  onMount(async () => {
    if (!browser) return;

    canvasThumb = document.createElement('canvas');
    canvasThumb.width = 100; canvasThumb.height = 100;
    ctxThumb = canvasThumb.getContext('2d', { alpha: false, desynchronized: true });

    try {
      status = "LOADING_WASM_CORE...";
      wasmModule = await loadWasmEngine();
      engine = new wasmModule.BiometricEngine();
      sharedBufferPtr = wasmModule._malloc(DIMS * 4);

      const mp = await import("@mediapipe/tasks-vision");
      // En tu onMount
const files = await mp.FilesetResolver.forVisionTasks(
  window.location.origin // Esto asegura que use http://tu-ip-o-dominio/
);

const faceLandmarker = await mp.FaceLandmarker.createFromOptions(files, {
  baseOptions: { 
    // Usamos URL absoluta local para evitar ambigüedades
    modelAssetPath: `${window.location.origin}/face_landmarker.task`, 
    delegate: "GPU" 
  },
  outputFaceBlendshapes: true,
  runningMode: "VIDEO",
  numFaces: 1
});

      status = "PROTOCOL_ALPHA_ACTIVE";
      addLog("ENGINE_LOADED", "info");
      startCamera(faceLandmarker);
    } catch (e) {
      status = "CORE_FAILURE";
      addLog("CRITICAL_WASM_ERROR", "warn");
    }
  });

  async function startCamera(faceLandmarker: any) {
    const stream = await navigator.mediaDevices.getUserMedia({ 
      video: { width: 1280, height: 720, frameRate: { ideal: 30 } } 
    });
    if (videoElement) {
      videoElement.srcObject = stream;
      videoElement.onloadedmetadata = () => {
        videoWidth = videoElement!.clientWidth;
        videoHeight = videoElement!.clientHeight;
        videoElement?.play();
        predict(faceLandmarker);
      };
    }
  }

  function predict(faceLandmarker: any) {
    if (!videoElement || !wasmModule || !sharedBufferPtr) return;
    const results = faceLandmarker.detectForVideo(videoElement, performance.now());
    
    if (results.faceLandmarks?.[0]) {
      const lms = results.faceLandmarks[0];
      const flatData = new Float32Array(lms.length * 3);
      for (let i = 0; i < lms.length; i++) {
        flatData[i * 3] = lms[i].x;
        flatData[i * 3 + 1] = lms[i].y;
        flatData[i * 3 + 2] = lms[i].z;
      }
      wasmModule.HEAPF32.set(flatData, sharedBufferPtr / 4);

      const analysis = engine.processFrame(sharedBufferPtr, lms.length);
      
      facePos = {
        x: (1 - analysis.minX) * videoWidth - ((analysis.maxX - analysis.minX) * videoWidth),
        y: analysis.minY * videoHeight,
        w: (analysis.maxX - analysis.minX) * videoWidth,
        h: (analysis.maxY - analysis.minY) * videoHeight
      };

      const shapes = results.faceBlendshapes[0]?.categories;
      if (shapes) {
        const scoreL = shapes.find(s => s.categoryName === 'eyeBlinkLeft')?.score || 0;
        const scoreR = shapes.find(s => s.categoryName === 'eyeBlinkRight')?.score || 0;
        const blinkScore = (scoreL + scoreR) / 2;
        const isCurrentlyBlinking = blinkScore > 0.45;

        if (isCurrentlyBlinking && !isBlinking) {
          isBlinking = true; blinkCount++;
          addLog(`LIVENESS_BEEP_${blinkCount}`, "info");
          if (blinkCount >= 2) {
            if (isRegistering) finalizeRegistration();
            else if (!authorizedUser) checkBiometrics();
          }
        } else if (!isCurrentlyBlinking) isBlinking = false;
      }

      if (isRegistering) handlePoseCapture(analysis.yaw, analysis.pitch, sharedBufferPtr);
    }
    requestAnimationFrame(() => predict(faceLandmarker));
  }

  function handlePoseCapture(yaw: number, pitch: number, ptr: number) {
    const cap = (key: keyof typeof anglesDone) => {
      rawPoseSamples.push(new Float32Array(wasmModule.HEAPF32.buffer, ptr, DIMS).slice());
      if (rawPoseSamples.length % 60 === 0) {
        anglesDone[key] = true;
        addLog(`VECTOR_${key.toUpperCase()}_CAPTURED`, "success");
      }
    };
    if (!anglesDone.left && yaw < -0.05) cap('left');
    else if (!anglesDone.right && yaw > 0.05) cap('right');
    else if (!anglesDone.up && pitch > 0.05) cap('up');
    else if (!anglesDone.down && pitch < -0.05) cap('down');
  }

  async function checkBiometrics() {
    if (isProcessing) return;
    isProcessing = true;
    status = "QUERYING_SQLITE_VAULT...";
    
    const db = await fetchBiometricDB();
    if (db.length === 0) {
      status = "NO_PROFILES_FOUND";
      isProcessing = false;
      return;
    }

    let match = null;
    let minDistance = 999;
    const mPtr = wasmModule._malloc(DIMS * 4);
    const iPtr = wasmModule._malloc(DIMS * 4);

    for (const user of db) {
      wasmModule.HEAPF32.set(new Float32Array(user.model.mean), mPtr / 4);
      wasmModule.HEAPF32.set(new Float32Array(user.model.invCovariance), iPtr / 4);
      const dist = engine.calculateMahalanobis(sharedBufferPtr, mPtr, iPtr, DIMS);
      if (dist < minDistance) { minDistance = dist; match = user; }
    }

    wasmModule._free(mPtr);
    wasmModule._free(iPtr);
    const rawScore = engine.getConfidenceScore(minDistance);
    precision = rawScore.toFixed(7);

    // Ajuste de Umbral a 95.00%
    if (match && rawScore >= 95.00) {
      authorizedUser = match;
      userThumb = match.thumb;
      status = "ACCESS_GRANTED_ALPHA";
      addLog(`AUTH_SUCCESS: ${precision}%`, "success");
    } else {
      status = "SECURITY_REJECT";
      addLog(`REJECTED: ${precision}%`, "warn");
    }
    isProcessing = false;
  }

  async function finalizeRegistration() {
    status = "ENCRYPTING_VAULT...";
    const tSize = rawPoseSamples.length * DIMS;
    const tPtr = wasmModule._malloc(tSize * 4);
    const tHeap = new Float32Array(wasmModule.HEAPF32.buffer, tPtr, tSize);
    rawPoseSamples.forEach((v, i) => tHeap.set(v, i * DIMS));

    const model = engine.trainModel(tPtr, rawPoseSamples.length, DIMS, crypto.randomUUID());
    
    const success = await syncProfileToDB({ 
      id: model.userId, 
      thumb: captureThumb(), 
      model: { 
        mean: Array.from(model.mean), 
        invCovariance: Array.from(model.invCovariance) 
      } 
    });

    wasmModule._free(tPtr);
    rawPoseSamples = []; 
    isRegistering = false;
    status = success ? "VAULT_SEALED" : "SYNC_ERROR";
    if (success) addLog("SQLITE_VAULT_UPDATED", "success");
  }

  function captureThumb(): string {
    if (ctxThumb && videoElement) {
      ctxThumb.drawImage(videoElement, 490, 210, 300, 300, 0, 0, 100, 100);
      return canvasThumb.toDataURL('image/webp', 0.7);
    }
    return "";
  }

  onDestroy(() => { if (wasmModule && sharedBufferPtr) wasmModule._free(sharedBufferPtr); });
</script>

<main class="scanner" role="main">
  <video bind:this={videoElement} aria-label="Escáner biométrico" muted playsinline></video>

  {#if authorizedUser}
    <div class="face-tag" style="left: {facePos.x}px; top: {facePos.y - 130}px; width: {facePos.w}px;">
      <div class="tag-content">
        <div class="thumb-container">
           <img src={userThumb} alt="Usuario" />
           <div class="scan-line"></div>
        </div>
        <div class="tag-data">
          <span class="user-id">ID: ALPHA_{authorizedUser.id.slice(0,4)}</span>
          <span class="conf" class:high-safety={parseFloat(precision) >= 95.00}>
            {precision}% MATCH
          </span>
          <div class="accuracy-bar-container">
            <div 
              class="accuracy-fill" 
              style="width: {precision}%; background: {parseFloat(precision) >= 95.00 ? '#0ff' : '#f00'};"
            ></div>
          </div>
        </div>
      </div>
      <div class="tag-line"></div>
    </div>
  {/if}

  <section class="side-terminal">
    <div class="terminal-header">ALPHA_LOG_STREAM</div>
    <div class="logs-container">
      {#each logs as log (log.time + log.msg)}
        <div class="log-entry {log.type}">
          <span class="log-time">[{log.time}]</span>
          <span class="log-msg">{log.msg}</span>
        </div>
      {/each}
    </div>
  </section>

  <nav class="ui">
    <div class="status" class:active={isProcessing} class:denied={status.includes('REJECT') || status.includes('BREACH') || status.includes('FAILED')}>
      {status}
    </div>
    <div class="hud">
      <div class="item">LIVENESS: {blinkCount}/2</div>
      {#if authorizedUser}
        <div class="item match">IDENTITY_CONFIRMED</div>
      {/if}
    </div>
    {#if !isRegistering && !authorizedUser}
      <button onclick={() => { isRegistering = true; status = "ROTATE_HEAD_FOR_CAPTURE"; blinkCount = 0; logs = []; }}>
        INITIALIZE_ENROLLMENT
      </button>
    {/if}
  </nav>
</main>

<style>
  @import url('https://fonts.googleapis.com/css2?family=Share+Tech+Mono&display=swap');

  .scanner { position: relative; width: 100vw; height: 100vh; background: #000; overflow: hidden; }
  video { width: 100%; height: 100%; object-fit: cover; filter: grayscale(1) brightness(0.6) contrast(1.3); transform: scaleX(-1); }

  .side-terminal {
    position: absolute; right: 25px; top: 50%; transform: translateY(-50%);
    width: 260px; background: rgba(0, 15, 15, 0.9); border-left: 2px solid #0ff;
    font-family: 'Share Tech Mono', monospace; padding: 12px; z-index: 20; backdrop-filter: blur(8px);
  }
  .terminal-header { font-size: 0.65rem; color: #0ff; border-bottom: 1px solid rgba(0, 255, 255, 0.2); margin-bottom: 10px; padding-bottom: 5px; letter-spacing: 2px; }
  .log-entry { font-size: 0.6rem; margin-bottom: 6px; display: flex; gap: 8px; animation: logSlide 0.2s ease-out; }
  .log-time { color: #555; }
  .info { color: #0ff; }
  .warn { color: #f00; }
  .success { color: #0f0; }

  .face-tag { position: absolute; pointer-events: none; transition: all 0.04s linear; display: flex; flex-direction: column; align-items: center; z-index: 10; }
  .tag-content { background: rgba(0, 15, 20, 0.95); border: 1px solid #0ff; backdrop-filter: blur(10px); display: flex; gap: 12px; padding: 10px; box-shadow: 0 0 30px rgba(0, 255, 255, 0.3); }
  .thumb-container { position: relative; width: 50px; height: 50px; border: 1px solid #0ff; overflow: hidden; }
  .thumb-container img { width: 100%; height: 100%; object-fit: cover; filter: grayscale(1) cyan; }
  .scan-line { position: absolute; width: 100%; height: 2px; background: #0ff; box-shadow: 0 0 10px #0ff; animation: scanLine 2s infinite linear; }
  .tag-data { display: flex; flex-direction: column; font-family: 'Share Tech Mono', monospace; width: 150px; }
  .user-id { color: #888; font-size: 10px; letter-spacing: 1px; }
  .conf { color: #ff0; font-size: 11px; margin-top: 2px; }
  .high-safety { color: #fff !important; text-shadow: 0 0 10px #0ff; }
  .accuracy-bar-container { width: 100%; height: 4px; background: #111; margin-top: 6px; }
  .accuracy-fill { height: 100%; transition: width 0.05s linear; }
  .tag-line { width: 1px; height: 60px; background: linear-gradient(to bottom, #0ff, transparent); }

  .ui { position: absolute; bottom: 60px; left: 50%; transform: translateX(-50%); display: flex; flex-direction: column; align-items: center; gap: 20px; z-index: 5; }
  .status { font-family: 'Share Tech Mono', monospace; font-size: 1.3rem; color: #0ff; text-shadow: 0 0 10px #0ff; letter-spacing: 3px; }
  .status.active { animation: pulse 0.8s infinite; }
  .status.denied { color: #f00; }
  .hud { display: flex; gap: 20px; background: rgba(0, 10, 10, 0.8); padding: 12px 25px; border-left: 3px solid #0ff; font-family: 'Share Tech Mono', monospace; color: #0ff; font-size: 0.75rem; }
  .match { color: #0f0; font-weight: bold; border-left: 1px solid #0f0; padding-left: 20px; }
  button { background: rgba(0, 255, 255, 0.05); border: 1px solid #0ff; color: #0ff; padding: 14px 40px; cursor: pointer; font-family: 'Share Tech Mono', monospace; transition: 0.4s; letter-spacing: 2px; }
  button:hover { background: #0ff; color: #000; box-shadow: 0 0 40px #0ff; }

  @keyframes scanLine { 0% { top: -10%; } 100% { top: 110%; } }
  @keyframes pulse { 0%, 100% { opacity: 1; } 50% { opacity: 0.3; } }
  @keyframes logSlide { from { opacity: 0; transform: translateX(20px); } to { opacity: 1; transform: translateX(0); } }
</style>