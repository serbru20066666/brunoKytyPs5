﻿# Builds the packages of the PS5 ports: <Game>.exe (brunoPortLauncher, with the game's icon and name), its picture,
# juego.ini, LEEME.txt, LICENCIA.txt and the hidden "motor" folder with the engine, its caches and (if asked) saves.
# Usage: empaquetar_ports_ps5.ps1 -Juego DemonsSouls|FC27|Astro -Destino <folder> [-ConPartidas] [-Publico [-Zip <file>]]
# -Publico: for a release: a neutral icon and no picture (no publisher art), no saves, no caches, no seed files (they
# hold the game's shader code), nothing of empaquetar_local.ps1, the engine built for any AVX2 CPU.
# A destination that exists is moved aside first (nothing is deleted).
param([Parameter(Mandatory)][ValidateSet('DemonsSouls', 'FC27', 'Astro')][string]$Juego,
      [Parameter(Mandatory)][string]$Destino, [switch]$ConPartidas, [string]$Version = '1.0.0', [switch]$Publico,
      [string]$Zip = '')
$ErrorActionPreference = 'Stop'
# brunoKytyPs5's ports folder, or the folder with brunoKytyPs5, brunoPortLauncher and kytyDemonsSouls side by side.
$enRepo   = Test-Path "$PSScriptRoot\launcher\CMakeLists.txt"
$ps4      = if ($enRepo) { Split-Path (Split-Path $PSScriptRoot) } else { $PSScriptRoot }
$kytyRepo = Join-Path $ps4 'brunoKytyPs5'
$fuente   = if ($enRepo) { "$PSScriptRoot\launcher" } else { "$ps4\brunoPortLauncher" }
$lanzador = "$fuente\_Build\brunoPortLauncher.exe"
$neutro   = "$fuente\recursos\icono.png"
$ds       = "$ps4\kytyDemonsSouls" # a worktree of the demons-souls branch
$dsBuild  = if ($Publico) { "$ds\_Build\windows-v3" } else { "$ds\_Build\windows" }
$kyty     = "$kytyRepo\_Build\windows-no-qt"
$fidelity = "$kytyRepo\3rdparty\ffx-api\bin\amd_fidelityfx_vk.dll" # AMD's signed FidelityFX
# This PC's own places and private settings, in empaquetar_local.ps1 next to this script (not in the repository):
# $arte (the games' pictures: <Arte>\icono.png, fondo.jpg), $portable (an install with saves and caches played so far),
# $buscarExtra[<Juego>] (more places for the game's folder), $entornoLocal[<Juego>] (engine variables of this PC's packages).
$arte = "$ps4\arte_ports"
$portable = $kytyRepo
$buscarExtra = @{}
$entornoLocal = @{}
if (Test-Path "$PSScriptRoot\empaquetar_local.ps1") { . "$PSScriptRoot\empaquetar_local.ps1" }
. "$PSScriptRoot\recursos_exe.ps1"

function Copiar([string]$desde, [string]$a, [string[]]$archivos = @('*')) {
	if (!(Test-Path $desde)) { throw "falta $desde" }
	New-Item -ItemType Directory -Force $a | Out-Null
	if ((Get-Item $desde).PSIsContainer) {
		robocopy $desde $a $archivos /E /NFL /NDL /NJH /NJS /NP | Out-Null
		if ($LASTEXITCODE -ge 8) { throw "robocopy $desde -> $a ($LASTEXITCODE)" }
	} else {
		Copy-Item $desde $a -Force
	}
}

# Buscar: where the game's folder may be, relative to the package (next to it, by default).
$juegos = @{
	DemonsSouls = @{ Titulo = "Demon's Souls"; Arte = 'Demons Souls'; Perfil = 'demons_souls'; Series = 'PPSA01340,PPSA01341,PPSA01342'
		Buscar = '..\Demons Souls (PPSA01342)' }
	FC27  = @{ Titulo = 'EA SPORTS FC 27'; Arte = 'FC 27'; Perfil = 'fc27'; Series = 'PPSA34015'
		Buscar = '..\FC 27 (PPSA34015)' }
	Astro = @{ Titulo = "ASTRO's PLAYROOM"; Arte = 'Astro Playroom'; Perfil = 'astro_playroom'; Series = 'PPSA01325'
		Buscar = '..\ASTROs PLAYROOM (PPSA01325)' }
}
$j = $juegos[$Juego]
if (!$Publico -and $buscarExtra[$Juego]) { $j.Buscar = (@($j.Buscar) + $buscarExtra[$Juego]) -join ';' }
$conArte = !$Publico -and (Test-Path "$arte\$($j.Arte)\icono.png")

if (Test-Path -LiteralPath $Destino) {
	$aparte = "$Destino (anterior $(Get-Date -Format 'yyyyMMdd-HHmmss'))"
	Move-Item -LiteralPath $Destino $aparte
	"existia: movido a $aparte"
}
New-Item -ItemType Directory -Force $Destino | Out-Null
$motor = Join-Path $Destino 'motor'
New-Item -ItemType Directory -Force $motor | Out-Null

# The program, named after its game, with its icon and name.
$exe = Join-Path $Destino "$($j.Titulo).exe"
Copy-Item $lanzador $exe
$icono = if ($conArte) { "$arte\$($j.Arte)\icono.png" } else { $neutro }
"recursos: " + [RecursosExe]::Apply($exe, $icono, $j.Titulo, $Version)
Copy-Item $icono (Join-Path $Destino 'icono.png')
if ($conArte -and (Test-Path "$arte\$($j.Arte)\fondo.jpg")) { Copy-Item "$arte\$($j.Arte)\fondo.jpg" $Destino }

$ini = @('# Lo lee el programa del juego. buscar: donde puede estar la carpeta del juego (las relativas, desde esta carpeta).',
	'[juego]', "titulo=$($j.Titulo)", 'plataforma=PlayStation 5', "perfil=$($j.Perfil)", "series=$($j.Series)",
	"buscar=$($j.Buscar)", 'motor=motor')
if (!$Publico -and $entornoLocal[$Juego]) { $ini += "entorno=$($entornoLocal[$Juego])" }
Set-Content (Join-Path $Destino 'juego.ini') -Encoding utf8 -Value $ini

if ($Juego -eq 'DemonsSouls') {
	# The Demon's Souls edition: its run-windows.ps1 starts the engine with its performance switches. A local package
	# takes this PC's precompiled shader store along (about 40 minutes saved); a public one, the hints only (no game code).
	foreach ($f in 'kyty_emulator.exe', 'kyty_shader_precompile.exe', 'libwinpthread-1.dll') { Copiar "$dsBuild\$f" $motor }
	Copiar $fidelity $motor
	foreach ($f in 'run-windows.ps1', 'run-windows.json', 'precompile-windows.ps1') { Copiar "$ds\$f" $motor }
	Copiar "$ds\tools\local\static-precompile" "$motor\tools\local\static-precompile" @('*.json', '*.hints', '*.files')
	if (!$Publico) {
		Copiar "$ds\_Build\static-precompile" $motor @('seeds-*', 'hinted-*', 'recorded-play-*', 'released-*')
		foreach ($d in 'static', 'local', 'warmup-v2', 'files') { Copiar "$ds\_PipelineCache\$d" "$motor\_PipelineCache\$d" }
	}
	if ($ConPartidas) { Copiar "$ds\_SaveData" "$motor\_SaveData" }
	$licencia = Get-Content "$ds\LICENSE" -Raw
} else {
	# FC 27 and ASTRO's PLAYROOM: brunoKytyPs5 itself, started with the command line the tests settled on.
	foreach ($f in 'kyty_emulator.exe', 'libwinpthread-1.dll') { Copiar "$kyty\$f" $motor }
	Copiar $fidelity $motor
	Copiar "$kyty\assets" "$motor\assets"
	$serie = $j.Series
	if (!$Publico) { Copiar "$portable\_PipelineCache" "$motor\_PipelineCache" @("$serie.shaders", "$serie.bin", "$serie.pipelines") }
	if ($Juego -eq 'Astro') { Copiar "$portable\_Patches" "$motor\_Patches" @('PPSA01325*.json') }
	if ($ConPartidas) {
		Copiar "$portable\_SaveData\$serie" "$motor\_SaveData\$serie"
		if (Test-Path "$portable\_DownloadData\$serie") { Copiar "$portable\_DownloadData\$serie" "$motor\_DownloadData\$serie" }
		if (Test-Path "$portable\_Trophies\$serie") { Copiar "$portable\_Trophies\$serie" "$motor\_Trophies\$serie" }
	}
	$licencia = Get-Content "$kytyRepo\LICENSE" -Raw
}
New-Item -ItemType Directory -Force "$motor\logs" | Out-Null
(Get-Item $motor).Attributes = 'Directory, Hidden'

Set-Content (Join-Path $Destino 'LICENCIA.txt') -Encoding utf8 -Value @(
	"$($j.Titulo) para PC: el juego necesita tu propia copia (la carpeta con eboot.bin). Este paquete no la incluye.", '',
	'El motor es KytyPS5 (licencia MIT, abajo); su programa, brunoPortLauncher. Componentes de terceros: SDL 3 (zlib),',
	'Dear ImGui (MIT), stb_image (dominio publico / MIT), la fuente Poppins (SIL Open Font License 1.1) y AMD FidelityFX',
	'(MIT, amd_fidelityfx_vk.dll firmado por AMD).', '', '----', '', $licencia)

$cache = if ($Publico) { 'la primera vez, pulsa "Precompilar shaders" (unos 40 minutos en 28 hilos; mas en procesadores con menos).' } else { 'este paquete ya los trae precompilados para tu tarjeta.' }
$notas = switch ($Juego) {
	'DemonsSouls' { @(
		"- Shaders: $cache Sin ellos el juego da tirones la primera vez que ve cada cosa.",
		'  Tras actualizar el driver de la tarjeta grafica, pulsa "Actualizar shaders": solo compila lo que cambio.',
		'- "Generacion de cuadros (FSR 3)" dobla los cuadros que se ven; "Mostrar FPS" (F8 en el juego) muestra los reales y los generados.') }
	'FC27' { @('- La primera vez que abre el menu puede tardar mientras prepara los shaders; despues va directo.',
		'- "Mostrar FPS" pone los cuadros por segundo arriba a la derecha.') }
	'Astro' { @('- "Resolucion interna" usa los parches del juego: 1440p es lo recomendado (estable y nitido con FSR).',
		'- "Mostrar FPS" pone los cuadros por segundo arriba a la derecha.') }
}
$donde = if ($Publico) { '"Carpeta del juego" (la que tiene eboot.bin). Si la pones junto a esta carpeta, la encuentra sola.' } else { '"Carpeta del juego" (la que tiene eboot.bin).' }
Set-Content (Join-Path $Destino 'LEEME.txt') -Encoding utf8 -Value (@(
	"$($j.Titulo) para PC", '', "Abre `"$($j.Titulo).exe`". La primera vez busca la carpeta del juego; si no la encuentra, eligela con",
	$donde, 'Ajusta lo principal, pulsa Aplicar o directamente Jugar.',
	'"Usar ajustes recomendados" vuelve a lo que mejor funciono en las pruebas (RX 6800 XT, i7-14700KF).', '') + $notas + @('',
	'Mando: DualSense, DualShock 4 o Xbox, conectados sin mas. Teclado: J = Cruz, L = Circulo, K = Cuadrado, I = Triangulo,',
	'flechas = cruceta, Intro = Options. F11 o Alt+Intro: pantalla completa.', '',
	'Las partidas, las caches y los registros estan en la carpeta oculta "motor" (Abrir registros los muestra).',
	'Acceso directo que entra al juego sin pasar por la pagina: anade --jugar al final del destino.'))
"paquete listo: $Destino"
Get-ChildItem -LiteralPath $Destino -Force | Select-Object Name, Length
if ($Zip) {
	# (.NET keeps the motor folder, hidden or not; the program hides it again when it opens.)
	Add-Type -AssemblyName System.IO.Compression.FileSystem
	if (Test-Path -LiteralPath $Zip) { Remove-Item -LiteralPath $Zip }
	[IO.Compression.ZipFile]::CreateFromDirectory($Destino, $Zip, [IO.Compression.CompressionLevel]::Optimal, $true)
	"zip: $Zip ({0:N1} MB)" -f ((Get-Item -LiteralPath $Zip).Length / 1MB)
}
