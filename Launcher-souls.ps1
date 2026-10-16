# KytyPS5 - Demon's Souls: lanzador (ventana). Launcher.cmd lo abre.
#   Jugar                -> arranca el juego; con "Frame generation AMD x2" (config\fsr3.json) o
#                           "x3" (config\fsr3-x3.json, 2 frames generados), si no, sin FG (config\sin-fg.json). Copia de las partidas antes.
#   Settings             -> carpeta del juego volcado (game-path.txt). La casilla se recuerda en launcher.ini.
Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing
[System.Windows.Forms.Application]::EnableVisualStyles()
$ErrorActionPreference = 'Stop'
$root     = $PSScriptRoot
$exeName  = 'kyty_emulator.exe'
$pathFile = Join-Path $root 'game-path.txt'
$iniFile  = Join-Path $root 'launcher.ini'
# Modos de frame generation: texto del desplegable -> config. Se recuerda el indice en launcher.ini.
$fgModes = @(
	@('Sin frame generation',                          'sin-fg.json'),
	@('---------- AMD (todas las graficas) ----------', ''),
	@('   Frame generation AMD x2',                     'fsr3.json'),
	@('   Frame generation AMD x3',                     'fsr3-x3.json'),
	@('   Frame generation AMD x4',                     'fsr3-x4.json'),
	@('   150 FPS AMD Smooth Souls',                    'fsr3-x3-150.json'),
	@('---------- NVIDIA (RTX) ----------',             ''),
	@('   Frame generation DLSS x2',                    'dlss-x2.json'),
	@('   Frame generation DLSS x3',                    'dlss-x3.json'),
	@('   Frame generation DLSS x4',                    'dlss-x4.json'),
	@('   120 FPS Nvidia Smooth Souls',                 'dlss-x3-120.json')
)
# Resolucion 3D (reescalado del propio juego a 4K): texto -> altura (run-windows.ps1 -RenderHeight). 1440 = la del juego.
$resModes = @(
	@('3D 1440p', 1440),
	@('3D 1260p', 1260),
	@('3D 1080p', 1080),
	@('3D 900p',  900),
	@('3D 720p',  720)
)
function Get-FgIndex {
	if (Test-Path -LiteralPath $iniFile) {
		$text = Get-Content -LiteralPath $iniFile -Raw
		if ($text -match 'fgmode=(\d+)') { return [int]$Matches[1] }
		if ($text -match 'framegen=1') { return 1 }
		if ($text -match 'framegen=0') { return 0 }
	}
	return 1
}
function Get-IniValue([string]$key, [string]$default) {
	if (Test-Path -LiteralPath $iniFile) {
		$text = Get-Content -LiteralPath $iniFile -Raw
		if ($text -match "(?m)^$key=(.*)$") { return $Matches[1].Trim() }
	}
	return $default
}
function Set-IniValue([string]$key, [string]$value) {
	$lines = @()
	if (Test-Path -LiteralPath $iniFile) {
		$lines = @(Get-Content -LiteralPath $iniFile | Where-Object { $_ -and ($_ -notmatch "^$key=") -and ($_ -notmatch '^framegen=') })
	}
	$lines += "$key=$value"
	[IO.File]::WriteAllText($iniFile, (($lines -join "`r`n") + "`r`n"), (New-Object System.Text.UTF8Encoding $false))
}
function Set-FgIndex([int]$i) { Set-IniValue 'fgmode' "$i" }

function Get-GamePath {
	if (Test-Path -LiteralPath $pathFile) { return ((Get-Content -LiteralPath $pathFile -Raw) -replace '^\uFEFF', '').Trim() }
	return ''
}
function Set-GamePath([string]$path) {
	[IO.File]::WriteAllText($pathFile, $path, (New-Object System.Text.UTF8Encoding $false))
}
function Backup-Partidas {
	# Copia de seguridad de las partidas antes de jugar. Solo copia; nunca borra nada.
	$src = Join-Path $root '_SaveData'
	if (Test-Path -LiteralPath $src) {
		$dst = Join-Path $root ('_SaveData-copias\' + (Get-Date -Format 'yyyyMMdd-HHmmss'))
		New-Item -ItemType Directory -Force $dst | Out-Null
		Copy-Item -Recurse -Force (Join-Path $src '*') $dst
	}
}
function Start-Game {
	if (-not (Get-GamePath) -or -not (Test-Path -LiteralPath (Get-GamePath))) {
		[System.Windows.Forms.MessageBox]::Show('Pon la carpeta del juego en Settings.', 'Launcher', 'OK', 'Warning') | Out-Null
		return
	}
	if (-not (Test-Path -LiteralPath (Join-Path $root $exeName))) {
		[System.Windows.Forms.MessageBox]::Show("Falta $exeName", 'Launcher', 'OK', 'Error') | Out-Null
		return
	}
	Backup-Partidas
	$config = $fgModes[[math]::Max(0, $cmbFg.SelectedIndex)][1]
	# Rutas entre comillas: Start-Process une los argumentos con espacios, y una carpeta con espacios
	# (C:\Users\Juan Perez\...) partia la ruta y PowerShell se cerraba sin arrancar nada.
	$psargs = @('-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', "`"$(Join-Path $root 'run-windows.ps1')`"",
	          '-Exe', "`"$(Join-Path $root $exeName)`"", '-Config', "`"$(Join-Path $root "config\$config")`"", '-Fullscreen')
	# HDR10 (solo si "Usar HDR" esta activado en la pantalla de Windows) y VSync (modo FIFO: sin tearing).
	if ($chkHdr.Checked)   { $psargs += '-Hdr' }
	if ($chkVsync.Checked) { $psargs += @('-PresentMode', 'Fifo') }
	# Con frame generation: vblank virtual de 360 Hz (run-windows -Vblank; corrige la velocidad del juego). Por
	# debajo de 60 cada frame sale a su tiempo (20 ms -> ~20, no 33 como a 60 Hz: los trompicones); el juego sigue
	# hasta 60 reales (FrameGenFloor), videos y menus a 60, y los Smooth (KYTY_FLIP_FPS 40/45) en vblanks justos.
	if ($config -ne 'sin-fg.json') { $psargs += @('-Vblank', '360') }
	$height = $resModes[[math]::Max(0, $cmbRes.SelectedIndex)][1]
	if ($height -ne 1440) { $psargs += @('-RenderHeight', "$height") }
	Start-Process -FilePath 'powershell' -ArgumentList $psargs -WorkingDirectory $root -WindowStyle Minimized | Out-Null
	$form.WindowState = 'Minimized'
}

# ---- ventana ----
$W = 1280; $H = 720
$form = New-Object System.Windows.Forms.Form
$form.Text            = 'Launcher'
$form.ClientSize      = New-Object System.Drawing.Size($W, $H)
$form.StartPosition   = 'CenterScreen'
$form.FormBorderStyle = 'FixedSingle'
$form.MaximizeBox     = $false
$form.BackColor       = [System.Drawing.Color]::FromArgb(14, 16, 22)
$fondo = Join-Path $root 'fondo-souls.jpg'
if (Test-Path -LiteralPath $fondo) {
	$form.BackgroundImage       = [System.Drawing.Image]::FromFile($fondo)
	$form.BackgroundImageLayout = 'Stretch'
}
$ico = Join-Path $root 'kyty_emulator.exe'
if (Test-Path -LiteralPath $ico) { try { $form.Icon = [System.Drawing.Icon]::ExtractAssociatedIcon($ico) } catch {} }

$titleFont  = New-Object System.Drawing.Font('Segoe UI Semibold', 40, [System.Drawing.FontStyle]::Bold)
$subFont    = New-Object System.Drawing.Font('Segoe UI', 12)
$buttonFont = New-Object System.Drawing.Font('Segoe UI Semibold', 15, [System.Drawing.FontStyle]::Bold)
$smallFont  = New-Object System.Drawing.Font('Segoe UI', 10)
$gold       = [System.Drawing.Color]::FromArgb(222, 196, 140)
$ink        = [System.Drawing.Color]::FromArgb(236, 232, 224)
$panelBack  = [System.Drawing.Color]::FromArgb(170, 8, 10, 14)
$pcFont     = New-Object System.Drawing.Font('Segoe UI Black', 34, [System.Drawing.FontStyle]::Bold)

function New-Label([string]$text, $font, $color, $x, $y, $w, $h, $align = 'MiddleLeft') {
	$l = New-Object System.Windows.Forms.Label
	$l.Text = $text; $l.Font = $font; $l.ForeColor = $color
	$l.BackColor = [System.Drawing.Color]::Transparent
	$l.Location = New-Object System.Drawing.Point($x, $y); $l.Size = New-Object System.Drawing.Size($w, $h)
	$l.TextAlign = $align
	return $l
}
function New-Button([string]$text, $x, $y, $w, $h, $primary) {
	$b = New-Object System.Windows.Forms.Button
	$b.Text = $text; $b.Font = $buttonFont
	$b.Location = New-Object System.Drawing.Point($x, $y); $b.Size = New-Object System.Drawing.Size($w, $h)
	$b.FlatStyle = 'Flat'; $b.FlatAppearance.BorderSize = 1
	$b.Cursor = [System.Windows.Forms.Cursors]::Hand
	if ($primary) {
		$b.BackColor = [System.Drawing.Color]::FromArgb(230, 176, 140, 70); $b.ForeColor = [System.Drawing.Color]::FromArgb(20, 16, 10)
		$b.FlatAppearance.BorderColor = $gold
		$b.FlatAppearance.MouseOverBackColor = [System.Drawing.Color]::FromArgb(240, 206, 170, 96)
	} else {
		$b.BackColor = [System.Drawing.Color]::FromArgb(200, 24, 28, 38); $b.ForeColor = $ink
		$b.FlatAppearance.BorderColor = [System.Drawing.Color]::FromArgb(110, 110, 100)
		$b.FlatAppearance.MouseOverBackColor = [System.Drawing.Color]::FromArgb(220, 44, 50, 64)
	}
	return $b
}

# ("PC" va pintado en la propia imagen de fondo.)

# Barra inferior semitransparente: Play, la casilla del frame generation, Settings y Salir.
$panel = New-Object System.Windows.Forms.Panel
$panel.Location  = New-Object System.Drawing.Point(0, ($H - 150))
$panel.Size      = New-Object System.Drawing.Size($W, 150)
$panel.BackColor = $panelBack
$form.Controls.Add($panel)

$btnPlay = New-Button 'PLAY' 50 38 260 74 $true
$btnPlay.Font = New-Object System.Drawing.Font('Segoe UI Black', 22, [System.Drawing.FontStyle]::Bold)
$btnPlay.Add_Click({ Start-Game })
$panel.Controls.Add($btnPlay)

$panel.Controls.Add((New-Label 'Frame generation' $smallFont $ink 350 30 300 22))
$cmbFg = New-Object System.Windows.Forms.ComboBox
$cmbFg.DropDownStyle = 'DropDownList'
$cmbFg.Font      = $subFont
$cmbFg.Location  = New-Object System.Drawing.Point(350, 56)
$cmbFg.Size      = New-Object System.Drawing.Size(300, 34)
foreach ($m in $fgModes) { [void]$cmbFg.Items.Add($m[0]) }
$cmbFg.SelectedIndex = [math]::Min([math]::Max(0, (Get-FgIndex)), $fgModes.Count - 1)
if (-not $fgModes[$cmbFg.SelectedIndex][1]) { $cmbFg.SelectedIndex = [math]::Min($cmbFg.SelectedIndex + 1, $fgModes.Count - 1) }
$cmbFg.Add_SelectedIndexChanged({
	# Section titles are not modes: move to the first mode under them.
	if ($cmbFg.SelectedIndex -ge 0 -and -not $fgModes[$cmbFg.SelectedIndex][1]) {
		$cmbFg.SelectedIndex = [math]::Min($cmbFg.SelectedIndex + 1, $fgModes.Count - 1)
		return
	}
	Set-FgIndex $cmbFg.SelectedIndex
})
$panel.Controls.Add($cmbFg)

function New-Check([string]$text, $x, $y, $w, [string]$key) {
	$c = New-Object System.Windows.Forms.CheckBox
	$c.Text = $text; $c.Font = $smallFont; $c.ForeColor = $ink
	$c.BackColor = [System.Drawing.Color]::Transparent
	$c.Location = New-Object System.Drawing.Point($x, $y); $c.Size = New-Object System.Drawing.Size($w, 26)
	$c.Checked = (Get-IniValue $key '0') -eq '1'
	$c.Add_CheckedChanged({ param($sender, $e) Set-IniValue $sender.Tag $(if ($sender.Checked) { '1' } else { '0' }) })
	$c.Tag = $key
	return $c
}
$chkHdr   = New-Check 'HDR' 350 100 90 'hdr'
$chkVsync = New-Check 'VSync' 450 100 100 'vsync'
$tips = New-Object System.Windows.Forms.ToolTip
$tips.SetToolTip($chkHdr, 'HDR10: activa antes "Usar HDR" en la configuracion de pantalla de Windows')
$tips.SetToolTip($chkVsync, 'VSync: sin cortes de imagen (tearing); con FreeSync/G-Sync sigue funcionando')
$panel.Controls.Add($chkHdr)
$panel.Controls.Add($chkVsync)

$cmbRes = New-Object System.Windows.Forms.ComboBox
$cmbRes.DropDownStyle = 'DropDownList'
$cmbRes.Font     = $smallFont
$cmbRes.Location = New-Object System.Drawing.Point(555, 100)
$cmbRes.Size     = New-Object System.Drawing.Size(100, 26)
foreach ($m in $resModes) { [void]$cmbRes.Items.Add($m[0]) }
$resIndex = 0
[void][int]::TryParse((Get-IniValue 'resmode' '0'), [ref]$resIndex)
$cmbRes.SelectedIndex = [math]::Min([math]::Max(0, $resIndex), $resModes.Count - 1)
$cmbRes.Add_SelectedIndexChanged({ Set-IniValue 'resmode' "$($cmbRes.SelectedIndex)" })
$tips.SetToolTip($cmbRes, 'Resolucion a la que el juego dibuja el 3D (la salida sigue en 4K). Menos = mas fps, menos nitidez.')
$panel.Controls.Add($cmbRes)

# Precompilar los shaders del juego (todos, de los archivos del juego): la primera vez puede tardar horas.
$btnPre = New-Button 'Precompilar shaders' ($W - 610) 46 230 58 $false
$btnPre.Add_Click({
	if (-not (Get-GamePath) -or -not (Test-Path -LiteralPath (Get-GamePath))) {
		[System.Windows.Forms.MessageBox]::Show('Pon la carpeta del juego en Settings.', 'Launcher', 'OK', 'Warning') | Out-Null
		return
	}
	$pre = Join-Path $root 'precompile-windows.ps1'
	if (-not (Test-Path -LiteralPath $pre)) { [System.Windows.Forms.MessageBox]::Show('Falta precompile-windows.ps1', 'Launcher', 'OK', 'Error') | Out-Null; return }
	$a = @('-NoExit', '-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', "`"$pre`"", '-Game', "`"$(Get-GamePath)`"")
	Start-Process -FilePath 'powershell' -ArgumentList $a -WorkingDirectory $root | Out-Null
})
$panel.Controls.Add($btnPre)

$btnSettings = New-Button 'Settings' ($W - 365) 46 160 58 $false
$panel.Controls.Add($btnSettings)

$btnExit = New-Button 'Salir' ($W - 190) 46 140 58 $false
$btnExit.Add_Click({ $form.Close() })
$panel.Controls.Add($btnExit)

# ---- Settings (panel desplegable) ----
$settings = New-Object System.Windows.Forms.Panel
$settings.Location  = New-Object System.Drawing.Point(($W - 650), ($H - 360))
$settings.Size      = New-Object System.Drawing.Size(610, 190)
$settings.BackColor = $panelBack
$settings.Visible   = $false
$form.Controls.Add($settings)
$settings.Controls.Add((New-Label 'Carpeta del juego volcado (PPSA01341)' $subFont $gold 20 16 560 28))
$pathBox = New-Object System.Windows.Forms.TextBox
$pathBox.Location  = New-Object System.Drawing.Point(20, 56); $pathBox.Size = New-Object System.Drawing.Size(450, 30)
$pathBox.Font      = $smallFont
$pathBox.Text      = Get-GamePath
$settings.Controls.Add($pathBox)
$btnBrowse = New-Button '...' 480 52 50 32 $false
$btnBrowse.Font = $smallFont
$btnBrowse.Add_Click({
	$dialog = New-Object System.Windows.Forms.FolderBrowserDialog
	$dialog.Description = 'Carpeta donde esta el juego volcado (la que contiene eboot.bin o sce_sys)'
	if ($pathBox.Text -and (Test-Path -LiteralPath $pathBox.Text)) { $dialog.SelectedPath = $pathBox.Text }
	if ($dialog.ShowDialog($form) -eq 'OK') { $pathBox.Text = $dialog.SelectedPath }
})
$settings.Controls.Add($btnBrowse)
$status = New-Label '' $smallFont $ink 20 140 560 24
$settings.Controls.Add($status)
$btnSave = New-Button 'Guardar' 20 96 160 36 $true
$btnSave.Font = $smallFont
$btnSave.Add_Click({
	$p = $pathBox.Text.Trim()
	if (-not $p -or -not (Test-Path -LiteralPath $p)) { $status.Text = 'Esa carpeta no existe.'; return }
	Set-GamePath $p
	$status.Text = 'Guardado en game-path.txt'
})
$settings.Controls.Add($btnSave)
$btnSettings.Add_Click({ $settings.Visible = -not $settings.Visible })

[void]$form.ShowDialog()
