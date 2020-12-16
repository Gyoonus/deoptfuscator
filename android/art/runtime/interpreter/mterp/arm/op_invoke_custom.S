%include "arm/invoke.S" { "helper":"MterpInvokeCustom" }
    /*
     * Handle an invoke-custom invocation.
     *
     * for: invoke-custom, invoke-custom/range
     */
    /* op vB, {vD, vE, vF, vG, vA}, call_site@BBBB */
    /* op vAA, {vCCCC..v(CCCC+AA-1)}, call_site@BBBB */
