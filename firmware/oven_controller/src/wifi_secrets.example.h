// Optional WiFi fallback credentials / Credenziali WiFi di fallback (facoltative)
//
// EN: The WiFi network is normally configured from the touch screen
//     (Settings > WiFi > Configure network) and stored in NVS. If you want the
//     firmware to know your network out of the box, copy this file to
//     wifi_secrets.h (ignored by git) and fill it in.
// IT: La rete si configura dal touch (Impostazioni > WiFi > Configura rete) e
//     resta salvata in NVS. Per avere una rete di fallback gia' nel firmware,
//     copia questo file in wifi_secrets.h (ignorato da git) e compilalo.
#pragma once

#define WIFI_SSID_DEFAULT     "my-network"
#define WIFI_PASSWORD_DEFAULT "my-password"
