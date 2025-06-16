import subprocess
import uvicorn
import atexit
from fastapi import FastAPI
from fastapi.responses import FileResponse
from pyngrok import ngrok

# --- Hardcoded Configuration ---
COMPILE_COMMAND = "qmk compile -kb toffee_studio/module_bare -km via"
FIRMWARE_FILE_PATH = "/home/ethanhaaan/Desktop/pcb/qmk_firmware/toffee_studio_module_bare_via.uf2"
PORT = 8080

# --- Server Code ---
app = FastAPI()

@app.get("/compile-and-get-firmware")
def compile_and_get_firmware():
    subprocess.run(COMPILE_COMMAND, shell=True, check=True)
    return FileResponse(path=FIRMWARE_FILE_PATH, media_type='application/octet-stream')

if __name__ == "__main__":
    public_url = ngrok.connect(PORT).public_url
    print("Server is running.")
    print(f"Public URL: {public_url}")
    atexit.register(ngrok.disconnect, public_url)
    uvicorn.run(app, host="0.0.0.0", port=PORT)
