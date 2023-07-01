<?php
use sxbind\ZoneParser;

require_once("guiconfig.inc");
require_once("config.inc");
require_once("/usr/local/pkg/sxbind_zoneparser.inc");
require_once("/usr/local/pkg/sxbind.inc");

$input_errors = array();
$savemsg = array();

if ($_REQUEST) {
    if ($_REQUEST["action"] == "get_zone_config") {
        if ($_REQUEST['id'] >= 0){
            $data = config_get_path('installedpackages/sxbindzone/config/' . $_REQUEST['id']);
        }
        
        if ($data){
            http_response_code(200);    // Content found
            header('Content-Type: application/json; charset=UTF-8');
            print(json_encode($data));
        }else{
            http_response_code(204);    // new content / empty
            header('Content-Type: application/json; charset=UTF-8');
            print(json_encode([]));
        }
        exit;
    }
    if ($_REQUEST["action"] == "validate_zone_config") {
        $data = $_REQUEST;
        header('Content-Type: text/plain; charset=UTF-8');
        // TODO: Validate Config

        // if ($data) {
        //     $rndckey = sxbind_get_key(null,'rndc_key');
        //     $result = sxbind_get_bind_conf_from_template($data, $rndckey, true);
           
        //     if ($result['success']){
        //         http_response_code(200);
        //     }else{
        //         http_response_code(400);
        //     }
            
        //     print($result['message']);
        // } else {
        //     http_response_code(500);
        //     header('Content-Type: text/plain;; charset=UTF-8');
        // }
        http_response_code(501);
        exit;
    }

    if ($_REQUEST["action"] == "save_zone_config") {
        $data = $_REQUEST;
        header('Content-Type: text/plain; charset=UTF-8');
        // if ($data) {
        //     $rndckey = sxbind_get_key(null,'rndc_key');
        //     $result = sxbind_get_bind_conf_from_template($data, $rndckey, true);

        //     if ($result['success']){
        //         http_response_code(200);
        //     }else{
        //         http_response_code(400);
        //     }

        //     print($result['message']);
        // } else {
        //     http_response_code(500);
        //     header('Content-Type: text/plain;; charset=UTF-8');
        // }
        http_response_code(501);
        exit;
    }
}

$pgtitle = array(gettext("Status"), gettext("Edit zone"));
$shortcut_section = "bind";

include("head.inc");

sxbind_display_top_tabs();

if ($input_errors) {
    print_input_errors($input_errors);
}

if (!empty($savemsg)) {
    $msgnew = '';
    foreach ($savemsg as $msg) {
        $msgnew .= htmlspecialchars($msg) . '<br/>';
    }
    print_info_box($msgnew, 'success');
}

?>

<form class="form-horizontal" method="post">
    <div class="panel panel-default">
        <div class="panel-heading">
            <h2 class="panel-title">UNDER DEVELLOPMENT !! -  DO NOT USE RIGHT NOW ---</h2>
        </div>
</div>

    <div class="panel panel-default">
        <div class="panel-heading">
            <h2 class="panel-title">Domain Zone Configuration</h2>
        </div>
        <div class="panel-body">
            <div class="form-group">
                <label for="disabled" class="col-sm-2 control-label">
                    <span>
                        <?= gettext('Disable This Zone') ?>
                    </span>
                </label>
                <div class="checkbox col-sm-10">
                    <label class="chkboxlbl"><input name="conf_disabled" id="conf_disabled" type="checkbox">
                        <?= gettext('Do not include this zone in sxbind config files.') ?>
                    </label>
                </div>
            </div>
            <div class="form-group">
                <label for="type" class="col-sm-2 control-label">
                    <span class="element-required">Zone Type</span>
                </label>
                <div class="col-sm-10">
                    <select class="form-control" name="conf_type" id="conf_type">
                        <option value="master" selected="">Master</option>
                        <option value="slave">Slave</option>
                        <option value="forward">Vorwärtsauflösung</option>
                        <option value="redirect">weiterleiten</option>
                    </select>

                    <span class="help-block">Select zone type.</span>
                </div>

            </div>
            <div class="form-group">
                <label for="reverso" class="col-sm-2 control-label">
                    <span>Reverse Zone</span>
                </label>
                <div class="checkbox col-sm-10">
                    <label class="chkboxlbl"><input name="conf_reverso" id="conf_reverso" type="checkbox"
                            onclick="javascript:enablechange();"> Check if this is a reverse zone.</label>
                </div>
            </div>
            <div class="form-group" style="display: none;">
                <label for="reversv6o" class="col-sm-2 control-label">
                    <span>IPv6 Reverse Zone</span>
                </label>
                <div class="checkbox col-sm-10">
                    <label class="chkboxlbl"><input name="conf_reversv6o" id="conf_reversv6o" type="checkbox"
                            disabled=""> Check if this is an IPv6 reverse zone. Reverse Zone must also be
                        enabled.</label>
                </div>
            </div>
            <div class="form-group" style="display: none;">
                <label for="converttoreverse" class="col-sm-2 control-label">
                    <span>IP for Reverse Zone</span>
                </label>
                <div class="col-sm-10">
                    <input class="form-control" name="conf_converttoreverse" id="conf_converttoreverse" type="text">
                    <span class="help-block">Enter the IP Address for this zone in regular CIDR notation and it will
                        automatically converted to reverse syntax.<br>
                        You may skip this field and enter the reverse name directly in the field "Zone Name".</span>
                </div>
            </div>
            <div class="form-group">
                <label for="conf_name" class="col-sm-2 control-label">
                    <span class="element-required">Zone Name</span>
                </label>
                <div class="col-sm-10">
                    <input class="form-control" name="conf_name" id="conf_name" type="text">
                    <span class="help-block">Enter the name for this zone (e.g. example.com)<br>
                        For reverse zones, use the IP in reverse order.<br>
                        <strong>Do not include .IN-ADDR.ARPA or .IPv6.ARPA as it will be automaticaly included in config
                            files when reverse zone option is checked.</strong></span>
                </div>
            </div>
            <div class="form-group">
                <label for="fullzonename" class="col-sm-2 control-label">
                    <span>Zone FQDN</span>
                </label>
                <div class="col-sm-10">
                    <input class="form-control" name="conf_fullzonename" id="conf_fullzonename" type="text"
                     readonly="readonly">
                </div>
            </div>
            <div class="form-group">
                <label for="description" class="col-sm-2 control-label">
                    <span>Beschreibung</span>
                </label>
                <div class="col-sm-10">
                    <input class="form-control" name="conf_description" id="conf_description" type="text">
                    <span class="help-block">Enter a description for this zone.</span>
                </div>
            </div>
            <div class="form-group">
                <label for="view[]" class="col-sm-2 control-label">
                    <span>Ansicht</span>
                </label>
                <div class="col-sm-10">
                    <select class="form-control" name="conf_view[]" id="conf_view[]" multiple="multiple">
                        <option selected="">Standard</option>
                    </select>
                    <span class="help-block">Select (CTRL+click) the views that this zone will belong to.</span>
                </div>
            </div>
            <div class="form-group">
                <label for="rpz" class="col-sm-2 control-label">
                    <span>Response Policy Zone</span>
                </label>
                <div class="checkbox col-sm-10">
                    <label class="chkboxlbl"><input name="conf_rpz" id="conf_rpz" type="checkbox"> Check if this zone
                        is used in a response policy.</label>
                </div>
            </div>
            <div class="form-group">
                <label for="custom" class="col-sm-2 control-label">
                    <span>Custom Option</span>
                </label>
                <div class="col-sm-10">
                    <textarea rows="5" class="form-control" name="conf_custom" id="conf_custom" cols="75"
                        style="width: auto;"></textarea>
                    <span class="help-block">You can put your own custom options here.</span>
                </div>
            </div>
        </div>
    </div>
    <div class="panel panel-default">
        <div class="panel-heading">
            <h2 class="panel-title">DNSSEC</h2>
        </div>
        <div class="panel-body">
            <div class="form-group">
                <label for="conf_dnssec" class="col-sm-2 control-label">
                    <span>Inline Signing</span>
                </label>
                <div class="checkbox col-sm-10">
                    <label class="chkboxlbl"><input name="conf_dnssec" id="conf_dnssec" type="checkbox"
                            onclick="javascript:enablechange();"> Enable inline DNSSEC signing</label>
                    <span class="help-block"> See <a
                            href="https://kb.isc.org/article/AA-00626/109/Inline-Signing-in-ISC-BIND-9.9.0-Examples.html">Inline
                            DNSSEC signing</a>.
                    </span>
                </div>
            </div>
            <div class="form-group">
                <label for="conf_backupkeys" class="col-sm-2 control-label">
                    <span>Backup Keys</span>
                </label>
                <div class="checkbox col-sm-10">
                    <label class="chkboxlbl"><input name="conf_backupkeys" id="conf_backupkeys" type="checkbox"
                            disabled=""> Enable this option to include all DNSSEC key files in XML.</label>
                </div>
            </div>
            <div class="form-group">
                <label for="conf_dsset" class="col-sm-2 control-label">
                    <span>DSSET</span>
                </label>
                <div class="col-sm-10">
                    <textarea rows="3" class="form-control" name="conf_dsset" id="conf_dsset" cols="75" style="width: auto;"
                        disabled="disabled"></textarea>
                    <span class="help-block">Digest fingerprint of the Key Signing Key for this zone.<br>
                        Upload this DSSET to your domain root server.</span>
                </div>
            </div>
        </div>
    </div>
    <div class="panel panel-default">
        <div class="panel-heading">
            <h2 class="panel-title">Slave Zone Configuration</h2>
        </div>
        <div class="panel-body">
            <div class="form-group" style="display: none;">
                <label for="conf_slaveip" class="col-sm-2 control-label">
                    <span>Master Zone IP</span>
                </label>
                <div class="col-sm-10">
                    <input class="form-control" name="conf_slaveip" id="conf_slaveip" type="text">
                    <span class="help-block">If this is a slave zone, enter the IP address of the master DNS
                        server.</span>
                </div>
            </div>
        </div>
    </div>
    <div class="panel panel-default">
        <div class="panel-heading">
            <h2 class="panel-title">Forward Zone Configuration</h2>
        </div>
        <div class="panel-body">
            <div class="form-group" style="display: none;">
                <label for="forwarders" class="col-sm-2 control-label">
                    <span>Forwarders</span>
                </label>
                <div class="col-sm-10">
                    <input class="form-control" name="conf_forwarders" id="conf_forwarders" type="text">
                    <span class="help-block">Enter forwarder IPs for this domain. Separate by semicolons (;).</span>
                </div>
            </div>
        </div>
    </div>
    <div class="panel panel-default">
        <div class="panel-heading">
            <h2 class="panel-title">Master Zone Configuration</h2>
        </div>
        <div class="panel-body">
            <div class="form-group">
                <label for="conf_tll" class="col-sm-2 control-label">
                    <span>TTL</span>
                </label>
                <div class="col-sm-10">
                    <input class="form-control" name="conf_tll" id="conf_tll" type="text">
                    <span class="help-block">Default expiration time of all resource records without their own TTL
                        value.</span>
                </div>
            </div>
            <div class="form-group">
                <label for="conf_nameserver" class="col-sm-2 control-label">
                    <span class="element-required">Name Server</span>
                </label>
                <div class="col-sm-10">
                    <input class="form-control" name="conf_nameserver" id="conf_nameserver" type="text">
                    <span class="help-block">Enter nameserver FQDN for this zone. (server.mydomain.com)</span>
                </div>
            </div>
            <div class="form-group">
                <label for="conf_nameserverip" class="col-sm-2 control-label">
                    <span>Name Server IP</span>
                </label>
                <div class="col-sm-10">
                    <input class="form-control" name="conf_nameserverip" id="conf_nameserverip" type="text">
                    <span class="help-block">Enter nameserver IP address. (IPv4 and IPv6 accepted)</span>
                </div>
            </div>
            <div class="form-group">
                <label for="ipns" class="col-sm-2 control-label">
                    <span>Base Domain IP</span>
                </label>
                <div class="col-sm-10">
                    <input class="form-control" name="conf_ipns" id="conf_ipns" type="text">
                    <span class="help-block">Enter IP address for base domain lookup. (Meaning, what IP should
                        <em>nslookup mydomain.com</em> return.)</span>
                </div>
            </div>
            <div class="form-group">
                <label for="conf_mail" class="col-sm-2 control-label">
                    <span>Mail Admin Zone</span>
                </label>
                <div class="col-sm-10">
                    <input class="form-control" name="conf_mail" id="conf_mail" type="text">
                    <span class="help-block">Enter mail admin zone.</span>
                </div>
            </div>
            <div class="form-group">
                <label for="conf_serial" class="col-sm-2 control-label">
                    <span>Seriennummer</span>
                </label>
                <div class="col-sm-10">
                    <input class="form-control" name="conf_serial" id="conf_serial" type="text">
                    <span class="help-block">Current Zone Serial. Will automatically filled if empty.</span>
                </div>
            </div>
            <div class="form-group">
                <label for="conf_refresh" class="col-sm-2 control-label">
                    <span>Aktualisieren</span>
                </label>
                <div class="col-sm-10">
                    <input class="form-control" name="conf_refresh" id="conf_refresh" type="text">
                    <span class="help-block">Slave refresh (Default: 1 day)</span>
                </div>
            </div>
            <div class="form-group">
                <label for="conf_retry" class="col-sm-2 control-label">
                    <span>Erneut versuchen</span>
                </label>
                <div class="col-sm-10">
                    <input class="form-control" name="conf_retry" id="conf_retry" type="text">
                    <span class="help-block">Slave retry time in case of a problem (Default: 2 hours)</span>
                </div>
            </div>
            <div class="form-group">
                <label for="conf_expire" class="col-sm-2 control-label">
                    <span>Verfallen</span>
                </label>
                <div class="col-sm-10">
                    <input class="form-control" name="conf_expire" id="conf_expire" type="text">
                    <span class="help-block">Slave expiration time (Default: 4 weeks)</span>
                </div>
            </div>
            <div class="form-group">
                <label for="minimum" class="col-sm-2 control-label">
                    <span>Minimum</span>
                </label>
                <div class="col-sm-10">
                    <input class="form-control" name="conf_minimum" id="conf_minimum" type="text">
                    <span class="help-block">Maximum caching time in case of failed lookups (Default: 1 hour)</span>
                </div>
            </div>
            <div class="form-group">
                <label for="allowupdate[]" class="col-sm-2 control-label">
                    <span>allow-update</span>
                </label>
                <div class="col-sm-10">
                    <select class="form-control" name="conf_allowupdate[]" id="conf_allowupdate[]" multiple="multiple">
                        <option value="none">nicht gesetzt</option>
                        <option value="any" selected="">alle</option>
                        <option value="localhost">localhost</option>
                        <option value="localnets">localnets</option>
                    </select>
                    <span class="help-block">Select(CTRL+click) who is allowed to send updates to this zone.<br>
                        The allow-update statement defines a match list of IP address(es) that are allowed&nbsp;
                        to submit dynamic updates for 'master' zones - i.e., it enables Dynamic DNS (DDNS).</span>
                </div>
            </div>
            <div class="form-group">
                <label for="enable_updatepolicy" class="col-sm-2 control-label">
                    <span>Enable update-policy</span>
                </label>
                <div class="checkbox col-sm-10">
                    <label class="chkboxlbl"><input name="conf_enable_updatepolicy" id="conf_enable_updatepolicy" type="checkbox"> Enable update-policy which overrides
                        allow-update.</label>
                    <span class="help-block">The update-policy statement replaces the allow-update statement.</span>
                </div>
            </div>
            <div class="form-group" style="display: none;">
                <label for="updatepolicy" class="col-sm-2 control-label">
                    <span>update-policy</span>
                </label>
                <div class="col-sm-10">
                    <input class="form-control" name="conf_updatepolicy" id="conf_updatepolicy" type="text" disabled="">
                    <span class="help-block">The update-policy statement defines the policy for submitting dynamic
                        updates to 'master' zones.<br>
                        <strong>Note: Do NOT include the surrounding { } when using multiple statements!</strong></span>
                </div>
            </div>
            <div class="form-group">
                <label for="allowquery[]" class="col-sm-2 control-label">
                    <span>allow-query</span>
                </label>
                <div class="col-sm-10">
                    <select class="form-control" name="conf_allowquery[]" id="conf_allowquery[]" multiple="multiple">
                        <option value="none">nicht gesetzt</option>
                        <option value="any" selected="">alle</option>
                        <option value="localhost">localhost</option>
                        <option value="localnets">localnets</option>
                    </select>
                    <span class="help-block">Select (CTRL+click) who is allowed to query this zone.<br>
                        The allow-query statement defines a match list of IP address(es) which are allowed to issue
                        queries to the server.</span>
                </div>
            </div>
            <div class="form-group">
                <label for="conf_allowtransfer[]" class="col-sm-2 control-label">
                    <span>allow-transfer</span>
                </label>
                <div class="col-sm-10">
                    <select class="form-control" name="conf_allowtransfer[]" id="conf_allowtransfer[]" multiple="multiple">
                        <option value="none">nicht gesetzt</option>
                        <option value="any" selected="">alle</option>
                        <option value="localhost">localhost</option>
                        <option value="localnets">localnets</option>
                    </select>
                    <span class="help-block">Select (CTRL+click) who is allowed to copy this zone.<br>
                        The allow-transfer statement defines a match list of IP address(es) that are allowed to
                        transfer&nbsp;
                        (copy) the zone information from the server (master or slave for the zone). While on its face
                        this may&nbsp;
                        seem an excessively friendly default, DNS data is essentially public (that's why its there) and
                        the bad guys&nbsp;
                        can get all of it anyway.<br><br>
                        However, if the thought of anyone being able to transfer your precious zone file is repugnant,
                        or&nbsp;
                        (and this is far more significant) you are concerned about possible DoS attack initiated by XFER
                        requests,&nbsp;
                        then you should use the following policy.</span>
                </div>
            </div>
        </div>
    </div>
    <div class="panel panel-default">
        <div class="panel-heading">
            <h2 class="panel-title">Zone Records</h2>
        </div>
        <div class="panel-body">
            <div class="form-group">
                <label class="col-sm-2 control-label">
                    <span>Info</span>
                </label>
                <div class="col-sm-10">
                    All Zone Records are Managed within the Zone Editor, which can be invoked after
                    initial Zone Creation.
                </div>
            </div>
        </div>
    </div>
    <div class="panel panel-default">
        <div class="panel-heading">
            <h2 class="panel-title">DEBUG</h2>
        </div>
        <div class="panel-body">
            <div class="form-group">
                <label for="conf_reset_zone" class="col-sm-2 control-label">
                    <span>Reset Zone</span>
                </label>
                <div class="checkbox col-sm-10">
                    <label class="chkboxlbl"><input name="conf_reset_zone" id="conf_reset_zone" type="checkbox" value="on">
                        ATTENTION: This option will delete the DB for the current zone durimng the next
                        update. Uso only if DB contains unresolvable errors.</label>
                </div>
            </div>
        </div>
    </div>
    <div class="col-sm-10 col-sm-offset-2">
        <button class="btn btn-primary" type="submit" name="action" id="btnSave">
            <i class="fa fa-save icon-embed-btn"> </i>Speichern
        </button>
    </div>

    <div id="dlg_updatestatus" class="modal fade" role="dialog" aria-labelledby="dlg_updatestatus" aria-hidden="true">
        <div class="modal-dialog modal-lg">
            <div class="modal-content">
                <div class="modal-header">
                    <button type="button" class="close" data-dismiss="modal" aria-label="Close">
                        <span aria-hidden="true">×</span>
                    </button>
                    <h3 class="modal-title">Aktion</h3>
                </div>
                <div class="modal-body">
                    <div class="form-group">
                        <label for="dlg_updatestatus_text" class="col-sm-2 control-label">
                        </label>
                        <div class="col-sm-8">
                            <textarea rows="10" class="row-fluid col-sm-10" name="dlg_updatestatus_text"
                                id="dlg_updatestatus_text" wrap="off">...Loading...</textarea>
                        </div>
                    </div>
                </div>
                <div class="modal-footer">
                    <input class="btn btn-primary" type="submit" value="close" data-dismiss="modal">
                </div>
            </div>
        </div>
    </div>
</form>
<?
include('foot.inc');
?>
<script type="text/javascript">
    var pendingChanges = false;

    function showMessage($text) {
        $('#dlg_updatestatus_text').text($text)
        $('#dlg_updatestatus_text').attr('readonly', true)
        $('#dlg_updatestatus').modal('show')
    }

    function showWait($text) {
        $('#dlg_updatestatus_text').text($text + '<br/><br/>Please wait for the process to complete.<br/><br/>This dialog will auto-close when the update is finished.<br/><br/>' +
            '<i class="content fa fa-spinner fa-pulse fa-lg text-center text-info"></i>')
        $('#dlg_updatestatus_text').attr('readonly', true)
        $('#dlg_updatestatus_text').modal('show')
    }

    function hideWait() {
        $('#dlg_updatestatus_text').modal('hide')
    }

    function getData(id) {
        data = {
            action: "get_zone_config",
            id: id
        }

        var p = new Promise((resolve, reject) => {
            showWait('load Data...')

            $.ajax(
                {
                    type: 'post',
                    data: data,
                    success: function (data) {
                        hideWait()
                        resolve(data)
                    },
                    error: function (data) {
                        hideWait();
                        showMessage(data.responseText)
                        reject(data.responseText)
                    }
                })
        })

        return p
    }

    function saveData(data, id) {
        data = {
            action: "save_zone_config",
            id: id,
            config: JSON.stringify(data)
        }

        var p = new Promise((resolve, reject) => {
            showWait('Saving Data...')

            $.ajax(
                {
                    type: 'post',
                    data: data,
                    success: function (data) {
                        hideWait()
                        resolve(data)
                        pendingChanges = false
                    },
                    error: function (data) {
                        hideWait();
                        showMessage(data.responseText)
                        reject(data.responseText)
                    }
                })
        })

        return p
    }

    events.push(function () {
        $("button[type=\"submit\"]").on("click", () => {
            $(this).button('loading')
        })

        $("button[value=\"save\"]").on('click', function () {
            var $btn = $(this)
            $btn.button('loading')
            var data = [];
            $("[name]").each(()=>{
                var key = $(this).attr('name')
                var val = $(this).val()
                if (key.startsWith("conf_")){    
                    ata[key.substring(5)] = val
                }
    d         
}           )
            saveData(data)

            // TODO: Save Data
            alert("not implemented.")

            $btn.button('reset')
        })

        $(window).on('beforeunload', function () {
            if (pendingChanges == true) {
                event.returnValue = `Discard changes, and leave Page?`
            }
        })

        // $("#showAdvanced").on("change", function () {
        //     if (this.checked) {
        //         $("[data-isadvancedoption=\"true\"]").show()
        //     } else {
        //         $("[data-isadvancedoption=\"true\"]").hide()
        //     }
        // })

        $("select[name]").on("change", function () {
            var key = $(this).attr('name')
            if (key.startsWith("conf_")){    
                pendingChanges == true
            }
        })
        $("input[name]").on("change", function () {
            var key = $(this).attr('name')
            if (key.startsWith("conf_")){    
                pendingChanges == true
            }
        })

        // Load initial data if available
        getData(<?=$_REQUEST['id']?>)
            .then((data) => {
                Object.entries(data).forEach(([key, value]) => {
                    $("[name=\"conf_" + key + "\"]").val(value)
                })
                pendingChanges = false
            })
    })
</script>
