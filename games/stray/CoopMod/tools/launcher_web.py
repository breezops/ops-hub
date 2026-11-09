"""Start the versioned WebView2 frontend, retaining the native fallback."""
import subprocess
def launch(release,home):
    executable=release/'tools/StrayCoopUI.exe'
    if not executable.is_file():return False
    try:
        check=subprocess.run([str(executable),'--check-runtime'],cwd=executable.parent,
            creationflags=subprocess.CREATE_NO_WINDOW,timeout=15)
        if check.returncode:return False
        subprocess.Popen([str(executable)],cwd=executable.parent)
        return True
    except (OSError,subprocess.TimeoutExpired):return False
