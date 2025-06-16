import requests
import sys

def download_firmware(server_url: str):
    target_url = f"{server_url}/compile-and-get-firmware"
    output_filename = "downloaded_firmware.uf2"

    print(f"Requesting firmware from {target_url}...")
    response = requests.get(target_url, stream=True)
    response.raise_for_status()

    with open(output_filename, "wb") as f:
        for chunk in response.iter_content(chunk_size=8192):
            f.write(chunk)

    print(f"Success! Firmware saved as '{output_filename}'")

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python firmware_downloader.py <ngrok_url>")
        sys.exit(1)

    ngrok_url = sys.argv[1]
    download_firmware(ngrok_url)
