Add-Type -AssemblyName System.Drawing

# Load the source PNG
$sourcePath = 'hdd-icon-512px.png'
if (-not (Test-Path $sourcePath)) {
    Write-Host 'Error: hdd-icon-512px.png not found!' -ForegroundColor Red
    exit 1
}

Write-Host 'Loading source image...' -ForegroundColor Cyan
$sourceImage = [System.Drawing.Image]::FromFile((Resolve-Path $sourcePath))

# Define the sizes needed for Windows 11
$sizes = @(256, 128, 64, 48, 32, 24, 16)

# Create a memory stream to build the ICO
$ms = New-Object System.IO.MemoryStream
$writer = New-Object System.IO.BinaryWriter($ms)

# ICO Header
$writer.Write([UInt16]0)       # Reserved. Must always be 0.
$writer.Write([UInt16]1)       # Image type: 1 for icon
$writer.Write([UInt16]$sizes.Length)  # Number of images

# Calculate offsets and prepare image data
$imageDataList = @()
$currentOffset = 6 + (16 * $sizes.Length)  # Header + directory entries

foreach ($size in $sizes) {
    Write-Host "Creating ${size}x${size} image..." -ForegroundColor Gray
    
    # Create resized bitmap
    $bitmap = New-Object System.Drawing.Bitmap($size, $size)
    $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
    $graphics.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
    $graphics.DrawImage($sourceImage, 0, 0, $size, $size)
    $graphics.Dispose()
    
    # Save to PNG stream
    $pngStream = New-Object System.IO.MemoryStream
    $bitmap.Save($pngStream, [System.Drawing.Imaging.ImageFormat]::Png)
    $pngData = $pngStream.ToArray()
    $pngStream.Dispose()
    $bitmap.Dispose()
    
    # Store image data for later
    $imageDataList += ,@{
        Size = $size
        Data = $pngData
        Offset = $currentOffset
    }
    
    $currentOffset += $pngData.Length
}

# Write directory entries
foreach ($imageData in $imageDataList) {
    $size = $imageData.Size
    $displaySize = if ($size -eq 256) { 0 } else { $size }
    
    $writer.Write([Byte]$displaySize)     # Width (0 = 256)
    $writer.Write([Byte]$displaySize)     # Height (0 = 256)
    $writer.Write([Byte]0)                # Color palette
    $writer.Write([Byte]0)                # Reserved
    $writer.Write([UInt16]1)              # Color planes
    $writer.Write([UInt16]32)             # Bits per pixel
    $writer.Write([UInt32]$imageData.Data.Length)  # Size of image data
    $writer.Write([UInt32]$imageData.Offset)       # Offset to image data
}

# Write image data
foreach ($imageData in $imageDataList) {
    $writer.Write($imageData.Data)
}

# Save the ICO file
$writer.Flush()
$icoPath = 'hdd-icon.ico'
[System.IO.File]::WriteAllBytes($icoPath, $ms.ToArray())

# Cleanup
$writer.Dispose()
$ms.Dispose()
$sourceImage.Dispose()

Write-Host "Successfully created $icoPath with $($sizes.Length) sizes" -ForegroundColor Green

# Verify the file
if (Test-Path $icoPath) {
    $fileInfo = Get-Item $icoPath
    Write-Host "File size: $($fileInfo.Length) bytes" -ForegroundColor Green
    exit 0
} else {
    Write-Host 'Error: Failed to create ICO file' -ForegroundColor Red
    exit 1
}