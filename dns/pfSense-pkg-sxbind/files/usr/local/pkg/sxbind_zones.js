events.push(function () {
    var $on_zonename_changed_timer = null

    $("input#reversv6o").closest(".form-group").hide()
    $("input#converttoreverse").closest(".form-group").hide()
    $("textarea#resultconfig").attr("disabled", true)
    $("textarea#dsset").attr("disabled", true)
    $("input#fullzonename").attr("readonly", "readonly")

    $("select#type").on('input', on_type_zone_changed)

    $("input#reverso").on('input', function () {
        on_reverse_changed()
    })

    $("input#reversv6o").on('input', function () {
        on_reverse_changed()
    })

    $("input#converttoreverse").on('input', on_zonename_changed)

    function on_reverse_changed() {
        if ($("input#reversv6o").is(':checked')) {
            $("input#reverso").prop("checked", true)
        }
        if (!$("input#reverso").is(':checked')) {
            $("input#reversv6o").prop("checked", "")
            $("input#converttoreverse").closest(".form-group").hide()
            $("input#reversv6o").closest(".form-group").hide()
            $("input#fullzonename").val($("input#name").val() + ".")
        } else {
            $("input#converttoreverse").closest(".form-group").show()
            $("input#reversv6o").closest(".form-group").show()
        }
    }
    function on_zonename_changed() {
        clearTimeout($on_zonename_changed_timer)

        if ($("input#reverso").is(':checked')) {
            $("input#name").val("[calculating name....]")
            $("input#fullzonename").val("[calculating name....]")
            $on_zonename_changed_timer = setTimeout(getPTR, 1000);
        } else {
            $("input#fullzonename").val($("input#name").val() + ".")
        }
    }

    function getPTR() {
        $.ajax({
            type: 'post',
            url: '/sxbind_tools.php',
            data: {
                action: "convert_ip_to_ptr",
                ip: $("#converttoreverse").val()
            },
            success: function (data) {
                data = data.toUpperCase();
                if (data.endsWith('.IP6.ARPA')) {
                    $("#reversv6o").prop("checked", "true")
                    $("#reversv6o").val(true);
                    $("input#fullzonename").val(data)
                    $("input#name").val(data.replace('.IP6.ARPA', ''))

                } else if (data.endsWith('.IN-ADDR.ARPA')) {
                    $("#reversv6o").prop("checked", "")
                    $("#reversv6o").val(false);
                    $("input#fullzonename").val(data)
                    $("input#name").val(data.replace('.IN-ADDR.ARPA', ''))

                } else {
                    $("input#fullzonename").val($("input#name").val())
                }
            },
            error: function (data) {
                $("input#fullzonename").val("[error]")
            }
        })
    }

    function on_type_zone_changed() {
        switch ($("select#type").val()) {
            case 'master':
                $("input#slaveip").closest(".form-group").hide()
                $("input#tll").closest(".form-group").show()
                $("input#nameserver").closest(".form-group").show()
                $("input#reverso").closest(".form-group").show()
                $("input#reversv6o").closest(".form-group").show()
                $("input#forwarders").closest(".form-group").hide()
                $("input#dnssec").closest(".form-group").show()
                $("input#backupkeys").closest(".form-group").show()
                $("input#regdhcpstatic").closest(".form-group").show()
                $("input#ipns").closest(".form-group").show()
                $("input#mail").closest(".form-group").show()
                $("input#serial").closest(".form-group").show()
                $("input#refresh").closest(".form-group").show()
                $("input#retry").closest(".form-group").show()
                $("input#expire").closest(".form-group").show()
                $("input#minimum").closest(".form-group").show()
                $("select#allowquery\\[\\]").closest(".form-group").show()
                $("select#allowupdate\\[\\]").closest(".form-group").show()
                $("select#allowtransfer\\[\\]").closest(".form-group").show()
                $("input#enable_updatepolicy").closest(".form-group").show()
                $("input#updatepolicy").closest(".form-group").hide()
                $("input#rpz").closest(".form-group").show()
                $("input#validate_zone").closest(".form-group").show()
                $("input#ddns_merging").closest(".form-group").show()
                $("input#increment_serial").closest(".form-group").show()
                break;
            case 'slave':
                $("input#slaveip").closest(".form-group").show()
                $("input#tll").closest(".form-group").hide()
                $("input#nameserver").closest(".form-group").hide()
                $("input#reverso").closest(".form-group").show()
                $("input#reversv6o").closest(".form-group").show()
                $("input#forwarders").closest(".form-group").hide()
                $("input#dnssec").closest(".form-group").show()
                $("input#backupkeys").closest(".form-group").show()
                $("input#regdhcpstatic").closest(".form-group").show()
                $("input#ipns").closest(".form-group").hide()
                $("input#mail").closest(".form-group").hide()
                $("input#serial").closest(".form-group").hide()
                $("input#refresh").closest(".form-group").hide()
                $("input#retry").closest(".form-group").hide()
                $("input#expire").closest(".form-group").hide()
                $("input#minimum").closest(".form-group").hide()
                $("select#allowquery\\[\\]").closest(".form-group").show()
                $("select#allowupdate\\[\\]").closest(".form-group").hide()
                $("select#allowtransfer\\[\\]").closest(".form-group").hide()
                $("input#enable_updatepolicy").closest(".form-group").hide()
                $("input#updatepolicy").closest(".form-group").hide()
                $("input#rpz").closest(".form-group").show()
                $("input#validate_zone").closest(".form-group").hide()
                $("input#ddns_merging").closest(".form-group").hide()
                $("input#increment_serial").closest(".form-group").hide()
                break;
            case 'forward':
                $("input#slaveip").closest(".form-group").hide()
                $("input#tll").closest(".form-group").hide()
                $("input#nameserver").closest(".form-group").hide()
                $("input#reverso").closest(".form-group").hide()
                $("input#reversv6o").closest(".form-group").hide()
                $("input#forwarders").closest(".form-group").show()
                $("input#dnssec").closest(".form-group").hide()
                $("input#backupkeys").closest(".form-group").hide()
                $("input#regdhcpstatic").closest(".form-group").hide()
                $("input#ipns").closest(".form-group").hide()
                $("input#mail").closest(".form-group").hide()
                $("input#serial").closest(".form-group").hide()
                $("input#refresh").closest(".form-group").hide()
                $("input#retry").closest(".form-group").hide()
                $("input#expire").closest(".form-group").hide()
                $("input#minimum").closest(".form-group").hide()
                $("select#allowquery\\[\\]").closest(".form-group").hide()
                $("select#allowupdate\\[\\]").closest(".form-group").hide()
                $("select#allowtransfer\\[\\]").closest(".form-group").hide()
                $("input#enable_updatepolicy").closest(".form-group").hide()
                $("input#updatepolicy").closest(".form-group").hide()
                $("input#rpz").closest(".form-group").hide()
                $("input#validate_zone").closest(".form-group").hide()
                $("input#ddns_merging").closest(".form-group").hide()
                $("input#increment_serial").closest(".form-group").show()
                break;
            case 'redirect':
                $("input#slaveip").closest(".form-group").hide()
                $("input#tll").closest(".form-group").hide()
                $("input#nameserver").closest(".form-group").show()
                $("input#reverso").closest(".form-group").hide()
                $("input#reversv6o").closest(".form-group").hide()
                $("input#forwarders").closest(".form-group").show()
                $("input#dnssec").closest(".form-group").hide()
                $("input#backupkeys").closest(".form-group").hide()
                $("input#regdhcpstatic").closest(".form-group").hide()
                $("input#ipns").closest(".form-group").hide()
                $("input#mail").closest(".form-group").show()
                $("input#serial").closest(".form-group").show()
                $("input#refresh").closest(".form-group").show()
                $("input#retry").closest(".form-group").show()
                $("input#expire").closest(".form-group").show()
                $("input#minimum").closest(".form-group").show()
                $("select#allowquery\\[\\]").closest(".form-group").show()
                $("select#allowupdate\\[\\]").closest(".form-group").hide()
                $("select#allowtransfer\\[\\]").closest(".form-group").hide()
                $("input#enable_updatepolicy").closest(".form-group").hide()
                $("input#updatepolicy").closest(".form-group").hide()
                $("input#rpz").closest(".form-group").hide()
                $("input#validate_zone").closest(".form-group").hide()
                $("input#ddns_merging").closest(".form-group").hide()
                $("input#increment_serial").closest(".form-group").show()
                break;
            default:
                break;
        }
    }

    on_type_zone_changed()
    on_reverse_changed()
});