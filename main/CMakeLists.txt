set(components "." "sntp" "prov" "mqtt" "sensors" "common")
set(dependencies bt esp_wifi nvs_flash wifi_provisioning mqtt esp_driver_gpio esp_adc json)
set(certs "certs/client.crt.pem" "certs/client.key.pem" "certs/ca.crt.pem")

idf_component_register(
    REQUIRES ${dependencies}
    INCLUDE_DIRS ${components}
    SRC_DIRS ${components}
    EMBED_TXTFILES ${certs}
)