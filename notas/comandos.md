# Comandos AquaSense

## Git
git add .
git commit -m "mensagem"
git push origin main

## Backend
cd "B:\Pasta Mestra\Projetos\AquaSense\AquaSense\backend"
node index.js

## Frontend
cd "B:\Pasta Mestra\Projetos\AquaSense\AquaSense\frontend"
npx serve . -p 8080

## Firmware
cd "B:\Pasta Mestra\Projetos\AquaSense\AquaSense\firmware\aquasense-firmware"
pio run
pio run --target upload