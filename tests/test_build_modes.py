"""Exercise project-level source guards without requiring an ESP toolchain."""
import os
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix="speed-config-") as temporary:
    scratch = Path(temporary)
    idf = scratch / "idf/tools/cmake"
    idf.mkdir(parents=True)
    for gps_build in (False, True):
        for gps_selected in (False, True):
            (idf / "project.cmake").write_text(
                "macro(project)\nendmacro()\n"
                f"set(CONFIG_SPEED_GPS_MODE {'ON' if gps_selected else 'OFF'})\n"
                f"set(CONFIG_SPEED_DEMO_MODE {'OFF' if gps_selected else 'ON'})\n"
            )
            result = subprocess.run(
                ["cmake", "-S", str(root), "-B", str(scratch / f"build-{gps_build}-{gps_selected}"),
                 "-DBOAT_PLATFORMIO=ON", f"-DBOAT_GPS_BUILD={'ON' if gps_build else 'OFF'}"],
                env={**os.environ, "IDF_PATH": str(scratch / "idf")},
                capture_output=True, text=True,
            )
            assert (result.returncode == 0) == (gps_build == gps_selected), result.stdout + result.stderr
print("PASS: demo/GPS environment selection cannot be overridden by persisted config")
