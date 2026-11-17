<#
.SYNOPSIS
  Compila e empacota o GPU-PRO para Windows x64 (pasta portátil + ZIP + SHA256).

.DESCRIPTION
  1. CMake (Visual Studio 2022): gpupro-native.dll e executores gpu-*-worker.exe.
  2. windeployqt nos executores (Qt usado só pelos workers; a interface é WinUI 3).
  3. dotnet publish da interface WinUI 3 autocontida (.NET 8 + Windows App SDK).
  4. Runtime C++ do Visual Studio (app-local), licenças e avisos de terceiros.
  5. Testes: portable-core-checks e GpuPro.Checks.
  6. ZIP portátil, instalador (Inno Setup 6, se instalado) e SHA256SUMS-windows-<versão>.txt em -Output.

.EXAMPLE
  ./windows/build-windows.ps1 -QtDir C:\Qt\6.8.3\msvc2022_64 -Glslang C:\glslang\bin\glslang.exe `
      -VulkanInclude C:\Vulkan-Headers\include -VulkanLibrary C:\vulkan-1.lib
#>
param(
  [string]$QtDir = $env:GPUPRO_QT_DIR,
  [string]$Glslang = $env:GPUPRO_GLSLANG,
  [string]$VulkanInclude = $env:GPUPRO_VULKAN_INCLUDE,
  [string]$VulkanLibrary = $env:GPUPRO_VULKAN_LIBRARY,
  [string]$CMake = $env:GPUPRO_CMAKE,
  [string]$Output,
  [switch]$SkipTests
)
$ErrorActionPreference = 'Stop'
$windows = $PSScriptRoot
$repo = Split-Path $windows -Parent
$build = Join-Path $windows 'build-workers'
$native = Join-Path $build 'Release'
if (-not $Output) { $Output = Join-Path $repo 'dist' }
function Step($text) { Write-Host "`n==> $text" -ForegroundColor Green }
function Need($path, $what) { if (-not $path -or -not (Test-Path $path)) { throw "$what não encontrado: '$path'" } }

$version = ([xml](Get-Content (Join-Path $windows 'GpuPro.WinUI\GpuPro.WinUI.csproj'))).Project.PropertyGroup.Version | Where-Object { $_ } | Select-Object -First 1
if (-not $version) { throw 'Versão não definida em GpuPro.WinUI.csproj' }
$name = "GPU-PRO-$version-windows-x64"
$stage = Join-Path $Output $name

# Valores já configurados em build-workers são reaproveitados quando nada é informado.
$cache = Join-Path $build 'CMakeCache.txt'
function Cached($key) { if (Test-Path $cache) { $line = Select-String -Path $cache -Pattern "^${key}:[A-Z]+=(.*)$" | Select-Object -First 1; if ($line) { return $line.Matches[0].Groups[1].Value } } }
if (-not $CMake) { $CMake = Cached 'CMAKE_COMMAND' }
if (-not $CMake) { $CMake = (Get-Command cmake -ErrorAction SilentlyContinue).Source }
if (-not $QtDir) { $qtCmake = Cached 'Qt6_DIR'; if ($qtCmake) { $QtDir = (Resolve-Path (Join-Path $qtCmake '..\..\..')).Path } }
Need $CMake 'CMake'

Step "Núcleo nativo e executores ($version)"
if (-not (Test-Path $cache)) {
  Need $QtDir 'Qt (msvc2022_64)'; Need $Glslang 'glslang'; Need $VulkanInclude 'Vulkan-Headers'; Need $VulkanLibrary 'vulkan-1.lib'
  & $CMake -S $repo -B $build -G 'Visual Studio 17 2022' -A x64 "-DCMAKE_PREFIX_PATH=$QtDir" "-DGLSLANG=$Glslang" "-DVulkan_INCLUDE_DIR=$VulkanInclude" "-DVulkan_LIBRARY=$VulkanLibrary"
  if ($LASTEXITCODE) { throw 'Falha na configuração CMake' }
}
& $CMake --build $build --config Release
if ($LASTEXITCODE) { throw 'Falha na compilação nativa' }

$deployQt = if ($QtDir) { Join-Path $QtDir 'bin\windeployqt.exe' }
if ($deployQt -and (Test-Path $deployQt)) {
  Step 'Bibliotecas Qt dos executores'
  foreach ($exe in Get-ChildItem $native -Filter 'gpu-*-worker.exe' | Where-Object Name -ne 'gpu-3d-worker.exe') {
    & $deployQt --release --no-translations --no-opengl-sw --no-system-d3d-compiler --no-compiler-runtime --dir $native $exe.FullName | Out-Null
    if ($LASTEXITCODE) { throw "windeployqt falhou em $($exe.Name)" }
  }
} elseif (-not (Test-Path (Join-Path $native 'Qt6Core.dll'))) { throw 'Qt não implantado: informe -QtDir' }

Step 'Interface WinUI 3'
if (Test-Path $stage) { Remove-Item $stage -Recurse -Force }
dotnet publish (Join-Path $windows 'GpuPro.WinUI\GpuPro.WinUI.csproj') -c Release -p:Platform=x64 -o $stage
if ($LASTEXITCODE) { throw 'Falha no dotnet publish' }
foreach ($file in 'GPU-PRO.exe', 'GPU-PRO.pri', 'App.xbf', 'MainWindow.xbf', 'gpupro-native.dll', 'gpu-bench-worker.exe', 'gpu-video-worker.exe', 'gpu-data-worker.exe', 'gpu-tuning-worker.exe', 'gpu-3d-worker.exe', 'Qt6Core.dll', 'platforms\qwindows.dll') {
  Need (Join-Path $stage $file) "Arquivo do pacote $file"
}

Step 'Runtime C++ (app-local), licenças e avisos'
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$vs = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$crt = Get-ChildItem (Join-Path $vs 'VC\Redist\MSVC\*\x64\Microsoft.VC14*.CRT') -Directory | Sort-Object FullName | Select-Object -Last 1
Need $crt 'Runtime C++ do Visual Studio (Microsoft.VC14x.CRT)'
Copy-Item (Join-Path $crt.FullName '*.dll') $stage
Copy-Item (Join-Path $repo 'LICENSE'), (Join-Path $repo 'NOTICE') $stage
Copy-Item (Join-Path $repo 'docs\licenses') (Join-Path $stage 'licenses') -Recurse
Copy-Item (Join-Path $repo 'docs\WINDOWS.md') (Join-Path $stage 'LEIA-ME-WINDOWS.md')

if (-not $SkipTests) {
  Step 'Testes'
  & (Join-Path $native 'portable-core-checks.exe'); if ($LASTEXITCODE) { throw 'portable-core-checks falhou' }
  dotnet run --project (Join-Path $windows 'checks\GpuPro.Checks.csproj') -c Release; if ($LASTEXITCODE) { throw 'GpuPro.Checks falhou' }
}

Step 'ZIP portátil'
$zip = Join-Path $Output "$name.zip"
if (Test-Path $zip) { Remove-Item $zip -Force }
Compress-Archive -Path (Join-Path $stage '*') -DestinationPath $zip -CompressionLevel Optimal
$packages = @($zip)

$iscc = @($env:GPUPRO_ISCC, (Join-Path ${env:ProgramFiles(x86)} 'Inno Setup 6\ISCC.exe'), (Join-Path $env:ProgramFiles 'Inno Setup 6\ISCC.exe')) | Where-Object { $_ -and (Test-Path $_) } | Select-Object -First 1
if ($iscc) {
  Step 'Instalador (Inno Setup)'
  & $iscc /Q "/DAppVersion=$version" "/DSourceDir=$stage" "/DOutputDir=$Output" (Join-Path $windows 'installer\GPU-PRO.iss')
  if ($LASTEXITCODE) { throw 'Falha ao gerar o instalador' }
  $packages += Join-Path $Output "$name-setup.exe"
} else { Write-Warning 'Inno Setup 6 não encontrado: instalador não gerado (somente ZIP).' }

Step 'SHA256'
$sums = foreach ($file in $packages) { "{0}  {1}" -f (Get-FileHash $file -Algorithm SHA256).Hash.ToLower(), (Split-Path $file -Leaf) }
$sums | Set-Content (Join-Path $Output "SHA256SUMS-windows-$version.txt") -Encoding ascii
$sums | ForEach-Object { Write-Host $_ -ForegroundColor Cyan }
Write-Host "`nPacotes em $Output" -ForegroundColor Cyan
