device_config = {
# Configuration syntax:
#
#  device: The value of ro.product.name or 'host' or 'jvm'
#  properties: (Use one or more of these).
#     * run-test-args: additional run-test-args
#
# *** IMPORTANT ***:
#    This configuration is used by the android build server. Targets must not be renamed
#    or removed.
#
##########################################
    # Fugu's don't have enough memory to support a 128m heap with normal concurrency.
    'aosp_fugu' : {
        'run-test-args': [ "--runtime-option", "-Xmx128m" ],
    },
    'fugu' : {
        'run-test-args': [ "--runtime-option", "-Xmx128m" ],
    },
}
