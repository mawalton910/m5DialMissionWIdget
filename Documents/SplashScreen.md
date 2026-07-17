# Changing the Splash Screen Image

## Requirements
- The new image must be exactly **240×240 pixels**
- Supported format: **JPEG (.jpg)**
- Keep the file size reasonable (under 200KB is fine)

## Steps

### 1. Prepare your image
- Resize/crop your logo to 240×240 pixels in any image editor
- Save it as a JPEG

### 2. Replace the file
- Copy your new JPEG to:
  ```
  I:\Scenario Writing\Vault 313\Airsoft\Secondary\mission_M5_refactored\data\Logosmall.jpg
  ```
  (overwrite the existing file)

### 3. Regenerate GuruLogo.h
Open **PowerShell** and paste this entire block, then press Enter:

```powershell
$bytes = [System.IO.File]::ReadAllBytes("I:\Scenario Writing\Vault 313\Airsoft\Secondary\mission_M5_refactored\data\Logosmall.jpg")
$lines = @()
$lines += "#pragma once"
$lines += "#include <pgmspace.h>"
$lines += ""
$lines += "const uint32_t guru_logo_jpg_len = $($bytes.Length);"
$lines += "const uint8_t guru_logo_jpg[] PROGMEM = {"
$chunks = for ($i = 0; $i -lt $bytes.Length; $i += 16) {
    $slice = $bytes[$i..([Math]::Min($i+15, $bytes.Length-1))]
    "  " + (($slice | ForEach-Object { "0x{0:X2}" -f $_ }) -join ", ") + ","
}
$lines += $chunks
$lines += "};"
[System.IO.File]::WriteAllLines("I:\Scenario Writing\Vault 313\Airsoft\Secondary\mission_M5_refactored\GuruLogo.h", $lines)
Write-Host "Done"
```

You should see **Done** printed when it finishes.

### 4. Flash the device
- Open the sketch in Arduino IDE
- Click **Upload** as normal

The new image will appear on the splash screen at startup.

---

## Notes
- The image displays for **3 seconds** at startup, then the device continues to the badge scan screen
- The splash screen also shows when exiting admin mode
- If the image looks upside down or mirrored, fix it in your image editor before step 1 — the converter outputs exactly what it reads


for audio
https://www.espboards.dev/tools/audio-grid-sequencer/