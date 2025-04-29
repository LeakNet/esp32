set(components "." "sntp" "prov" "mqtt" "sensors" "common" "proto")
set(dependencies bt esp_wifi nvs_flash wifi_provisioning mqtt esp_driver_gpio esp_adc nanopb esp_pm ulp)
set(certs "certs/client.crt.pem" "certs/client.key.pem" "certs/ca.crt.pem")

# ULP
set(ulp_app_name ulp_${COMPONENT_NAME})
set(ulp_s_sources "ulp/pulse_cnt.S" "ulp/wake_up.S")
set(ulp_exp_dep_srcs "main.c")

idf_component_register(
    REQUIRES ${dependencies}
    INCLUDE_DIRS ${components}
    SRC_DIRS ${components}
    EMBED_TXTFILES ${certs}
)

ulp_embed_binary(${ulp_app_name} "${ulp_s_sources}" "${ulp_exp_dep_srcs}")