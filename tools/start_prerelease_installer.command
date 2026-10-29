#!/bin/bash
set -e
cd -- "$(dirname "$0")"
if ! command -v python3 >/dev/null 2>&1; then
  echo 'Bitte Python 3 installieren und den Starter erneut öffnen.'
  read -r
  exit 1
fi
echo 'Caravel 0.7.0-rc1 Installer: http://127.0.0.1:8070/webapp/installer/'
echo 'FM-1 per USB anschließen. Dieses Terminal während des Updates offen lassen.'
echo 'Zum Beenden nach dem Update: Strg+C.'
(
  sleep 1
  open -a 'Google Chrome' 'http://127.0.0.1:8070/webapp/installer/' || true
) &
exec python3 -m http.server 8070 --bind 127.0.0.1 --directory Installer
