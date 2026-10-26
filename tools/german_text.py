"""The old name of tools/add_language.py, kept so older notes and commands still work.

Without --language it puts in German, as it always did under this name.
"""

import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from add_language import main

if __name__ == "__main__":
    if "--restore" not in sys.argv and not any(a == "--language" or a.startswith("--language=") for a in sys.argv):
        sys.argv += ["--language", "de"]
    main()
