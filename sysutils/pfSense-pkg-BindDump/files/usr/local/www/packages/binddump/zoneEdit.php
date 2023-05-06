<?php
/*
 * binddump.php
 */
require_once("guiconfig.inc");
require_once("config.inc");
require_once("/usr/local/pkg/binddump.inc");

define('BIND_LOCALBASE', '/usr/local');
define('CHROOT_LOCALBASE', '/var/etc/named');
$rndc_conf_path = BIND_LOCALBASE . "/etc/rndc.conf";
$rndc = "/usr/local/sbin/rndc -q -c {$rndc_conf_path}";


if ($_POST) {
    if (!empty($_POST['getList'])) {
        try {
            $selectedZone = explode('__', $_POST['zone']);
            $zoneview = $selectedZone[0];
            $zonename = $selectedZone[1];
            $zonename_reverse = $selectedZone[2];
            $zonetype = $selectedZone[3];

            $zone_data = binddump_compilezone($zoneview, $zonename_reverse, $zonetype);
            $zone_data_parsed = binddump_parse_rndc_zone_dump($zone_data, $zonename_reverse, false);
            echo json_encode($zone_data_parsed, JSON_UNESCAPED_UNICODE);
            exit;
        } catch (Exception $e) {
            die($e->getMessage());
        }
    }

    $input_errors = array();
    $post = $_POST;

    if ($_POST['zoneselect'] !== $post['current_zone'] || !empty($_POST["reload"])) {
        if ($_POST['zone_editable'] == "true") {
            $input_errors[] = "Zone is in Edit-Mode. End Edit-Mode before switching to another Zone.";
            $post['zoneselect'] = $post['current_zone'];
        } else {
            $loadZone = true;
            $post['zone_editable'] = "false";
            $post['current_zone'] = $post['zoneselect'];
        }
    }

    $selectedZone = explode('__', htmlspecialchars_decode($post['current_zone']));
    $zoneview = $selectedZone[0];
    $zonename = $selectedZone[1];
    $zonename_reverse = $selectedZone[2];
    $zonetype = $selectedZone[3];


    if ($loadZone) {
        try {
            $post['zone_data'] = binddump_compilezone($zoneview, $zonename_reverse);
        } catch (Exception $e) {
            $zrev = binddump_re_reverse_zonename($zonename_reverse);
            $post['zone_data'] = $zrev; //'[error]';
            $input_errors[] = $e->getMessage();
            unset($_POST["save"]);
            $post['zone_editable'] = "false";
        }
    }

    if (!empty($_POST["thawall"])) {
        exec("{$rndc} thaw" . ' 2>&1', $output, $resultCode);
        if ($resultCode !== 0) {
            $input_errors[] = "RNDC THAW throwed an exception. Code {$resultCode} \n " . implode("\n", $output);
        } else {
            $post['zone_editable'] = "false";
            $savemsg = "Thaw successfull.\n" . implode("\n", $output);
        }
    }

    if (!empty($_POST["save"])) {
        $tempDB = tempnam("/tmp", "validate_zone");
        file_put_contents($tempDB, $post['zone_data']);

        // validate and save to DB if successfull.
        exec('/usr/local/sbin/named-checkzone -F text ' .
            '-o ' . escapeshellarg(CHROOT_LOCALBASE . "/etc/namedb/{$zonetype}/{$zoneview}/{$zonename}.DB") . ' ' .
            escapeshellarg($zonename_reverse) . ' ' .
            escapeshellarg($tempDB) . ' 2>&1', $output, $resultCode);

        unlink($tempDB);

        if ($resultCode !== 0) {
            $input_errors[] = "named-checkzone throwed an exception. Code {$resultCode} \n " . implode("\n", $output);
        } else {
            $savemsg = implode("\n", $output);
            exec("{$rndc} thaw " . escapeshellarg($zonename_reverse) . " IN " . escapeshellarg($zoneview) . ' 2>&1', $output, $resultCode);
            $post['zone_editable'] = "false";
        }
    }

    if (!empty($_POST["thaw"])) {
        exec("{$rndc} thaw " . escapeshellarg($zonename_reverse) . " IN " . escapeshellarg($zoneview) . ' 2>&1', $output, $resultCode);
        if ($resultCode !== 0) {
            $input_errors[] = "RNDC THAW throwed an exception. Zone {$zonename_reverse} may still be frozen. Code {$resultCode} \n " . implode("\n", $output);
        } else {
            $post['zone_editable'] = "false";
            $savemsg = "Thaw successfull.\n\n" . implode("\n", $output);
        }
    }

    if (!empty($_POST["freeze"])) {
        exec("{$rndc} freeze " . escapeshellarg($zonename_reverse) . " IN " . escapeshellarg($zoneview) . ' 2>&1', $output, $resultCode);

        if ($resultCode !== 0) {
            $input_errors[] = "named-checkzone throwed an exception. Code {$resultCode} \n " . implode("\n", $output);
        } else {
            try {
                $post['zone_data'] = binddump_compilezone($zoneview, $zonename_reverse);
                $post['zone_editable'] = "true";
            } catch (Exception $e) {
                $post['zone_data'] = '[error]';
                $input_errors[] = $e->getMessage();
                $post['zone_editable'] = "false";
            }

            $savemsg = implode("\n", $output) . "\n\n Zone frozen and file reloaded.\n Don't forget to END EDIT before leaving.";
        }


    }
}

if ($post['zone_data']) {
    $zone_data_parsed = binddump_parse_rndc_zone_dump($post['zone_data'], $zonename_reverse, false);
} else {
    $zone_data_parsed = [];
}

$pgtitle = array(gettext("Status"), gettext("Edit zone"));
$shortcut_section = "bind";

include("head.inc");

$tab_array = array();
$tab_array[] = array(gettext("Database"), false, "/packages/binddump/binddump.php");
$tab_array[] = array(gettext("Edit RAW Zone File"), true, "/packages/binddump/zoneEdit.php");
display_top_tabs($tab_array);

$zonelist = [];
foreach (binddump_get_zonelist() as $zone) {
    if ($zone['type'] == 'master') {
        $zonelist[$zone['view'] . '__' . $zone['name'] . '__' . binddump_reverse_zonename($zone) . '__' . $zone['type']] = binddump_reverse_zonename($zone) . '  (' . $zone['view'] . ')';
    }
}
ksort($zonelist);

if ($input_errors) {
    print_input_errors($input_errors);
}

if ($savemsg) {
    print_info_box($savemsg, 'success');
}

?>
<style>
    [data-status="deleted"] {
        text-decoration: line-through;
        opacity: 0.5;
    }

    [data-status="changed"] {
        font-weight: bold;
        ;

    }

    [data-status="added"] {
        font-weight: bold;
        ;

    }
</style>


<form class="form-horizontal" method="post" action="zoneEdit.php" enctype="multipart/form-data">
    <div class="panel panel-default">
        <div class="panel-heading">
            <h2 class="panel-title">
                <?= gettext('Zone') ?>
            </h2>
        </div>
        <div class="panel-body">
            <div class="form-group">
                <label for="zoneselect" class="col-sm-2 control-label">
                    <span>
                        <?= gettext('Zone') ?>
                    </span>
                </label>
                <div class="col-sm-10">
                    <select class="form-control" name="zoneselect" id="zoneselect" onchange="this.form.submit();">
                        <option value="">
                            <?= gettext('Select Zone...') ?>
                        </option>
                        <? foreach ($zonelist as $key => $value) { ?>
                            <option <? if ($key == $post['current_zone']) {
                                print('selected');
                            } ?> value="<?= $key ?>"><?= $value ?>
                            </option>
                        <? } ?>
                    </select>
                </div>
            </div>
            <div class="form-group">
                <label for="load" class="col-sm-2 control-label">
                    <span>
                        <?= gettext('Actions') ?>
                    </span>
                </label>
                <div class="col-sm-2">
                    <input class="btn btn-primary" type="submit" value="<?= gettext('Reload Zone') ?>" name="load"
                        id="load">
                </div>
                <div class="col-sm-2">
                    <input class="btn btn-primary" type="submit" value="<?= gettext('Start Edit') ?>" name="freeze"
                        id="freeze">
                    <span class="help-block">
                        <?= gettext('While in Edit-Mode, DDNS Updates are disabled.') ?>
                    </span>
                </div>
                <div class="col-sm-2">
                    <input class="btn btn-primary" type="submit" value="<?= gettext('End Edit') ?>" name="thaw"
                        id="thaw">
                </div>
                <div class="col-sm-2">
                    <input class="btn btn-primary" type="submit"
                        value="<?= gettext('End Edit') ?> -- <?= gettext('All Zones') ?>" name="thawall" id="thawall">
                </div>
            </div>
        </div>
    </div>

    <div class="panel panel-default">
        <div class="panel-heading">
            <h2 class="panel-title">
                <?= gettext('Edit Zone File') ?>
            </h2>
        </div>
        <div class="panel-body">
            <textarea rows="10" class="col-sm-12 form-control" name="zone_data" id="zone_data" wrap="off" <? if ($post['zone_editable'] !== 'true') {
                print('readonly="readonly"');
            } ?>><?= $post['zone_data'] ?></textarea>
        </div>
    </div>

    <div class="panel-body table-responsive">
        <table id="zonerecordlist" class="table table-striped table-hover table-condensed sortable-theme-bootstrap"
            data-sortable>
            <thead>
                <tr data-keyfieldname="hash">
                    <th data-fieldname="name">
                        <?= gettext("Name") ?>
                    </th>
                    <th data-fieldname="type">
                        <?= gettext("Type") ?>
                    </th>
                    <th data-fieldname="rdata">
                        <?= gettext("Data") ?>
                    </th>
                </tr>
            </thead>
            <tbody>

            </tbody>
        </table>
        <div class="col-sm-10 col-sm-offset-2">
            <button class="btn btn-primary" type="button" value="<?= gettext('Save') ?>" name="save" id="save" onclick="submitChanges();"><i
                    class="fa fa-save icon-embed-btn"> </i>
                <?= gettext('Save') ?>
            </button>
            <button class="btn btn-primary" type="button" value="<?= gettext('Add') ?>" id="btnNewRow"><i
                    class="fa fa-add icon-embed-btn"> </i>
                <?= gettext('Add') ?>
            </button>
        </div>
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
                    <input class="btn btn-primary" type="submit" value="Schliessen" name="save" id="save"
                        data-dismiss="modal">
                </div>
            </div>
        </div>
    </div>
    <div id="dlg_wait" class="modal fade" role="dialog" aria-labelledby="dlg_wait" aria-hidden="true">
        <div class="modal-dialog modal-sm">
            <div class="modal-content">
                <div class="modal-header">
                    <button type="button" class="close" data-dismiss="modal" aria-label="Close">
                        <span aria-hidden="true">×</span>
                    </button>
                    <h3 class="modal-title">Please wait</h3>
                </div>
                <div class="modal-body">
                    <div class="form-group">
                        <label class="col-sm-8 control-label">
                            <span>dlg_wait_text</span>
                        </label>
                        <div class="col-sm-10">
                            Please wait for the process to complete.<br><br>This dialog will auto-close when the update
                            is finished.<br><br><i
                                class="content fa fa-spinner fa-pulse fa-lg text-center text-info"></i>
                        </div>

                    </div>
                </div>
                <div class="modal-footer">
                    <input class="btn btn-primary" type="submit" value="Schliessen" name="save" id="save"
                        data-dismiss="modal">
                </div>
            </div>
        </div>
    </div>

</form>
<?
include('foot.inc');
?>
<script src="sxTable.js"></script>
<script type="text/javascript">
    var table1;

    function showMessage($text) {
        $('#dlg_updatestatus_text').text($text);
        $('#dlg_updatestatus_text').attr('readonly', true);
        $('#dlg_updatestatus').modal('show');
    }
    function showWait($text) {
        $('#dlg_wait_text').text($text + '<br/><br/>Please wait for the process to complete.<br/><br/>This dialog will auto-close when the update is finished.<br/><br/>' +
            '<i class="content fa fa-spinner fa-pulse fa-lg text-center text-info"></i>');
        $('#dlg_wait_text').attr('readonly', true);
        $('#dlg_wait').modal('show');
    }
    function hideWait() {
        $('#dlg_wait').modal('hide');
    }

    function submitChanges(){
        showWait('Saving Zone Data');

        var data = table1.sxTable("getChangeSet");
        alert(JSON.stringify(data));

       
        //TODO: Save changes.....
        table1.sxTable("mergeChanges");
        hideWait();
    }
    function reloadData() {
        var zone = $('#zoneselect').find(":selected").val();
        showWait('Loading new Zone Data');

        table1.sxTable("clear");

        $.ajax(
            {
                type: 'post',
                data: {
                    getList: 'true',
                    zone: zone
                },
                success: function (data) {
                    hideWait();
                    if (data.startsWith('[')) {
                        table1.sxTable("fromJson", data);
                    } else {
                        showMessage(data);
                    }
                },
                error: function (data) {
                    hideWait();
                    showMessage(data.responseText);
                }
            });
    }

    events.push(function () {
        // $(window).on('beforeunload', function () {
        //     $.ajax({
        //         url: 'zoneEdit.php',
        //         type: 'POST',
        //         data: {
        //             'thawall': 'thawall'
        //         }
        //     });
        // });

        table1 = $('#zonerecordlist').sxTable({
            columns: [
                {
                    fieldname: "name",
                    caption: "Name",
                    editable: true
                },
                {
                    fieldname: "type",
                    caption: "Type",
                    editable: true
                },
                {
                    fieldname: "rdata",
                    caption: "RData",
                    editable: true
                },
                {
                    fieldname: "_buttons",
                    caption: "",
                    editable: false
                },
            ],
            templateEditor: function (e) {
                if (e.fieldname == "type") {
                    var dat = $('<select>');
                    dat.append($("<option />").val('A').text('A'));
                    dat.append($("<option />").val('AAAA').text('AAAA'));
                    dat.append($("<option />").val('TXT').text('TXT'));
                    dat.append($("<option />").val('SPF').text('SPF'));
                    dat.append($("<option />").val('MX').text('MX'));
                    dat.append($("<option />").val('NS').text('NS'));
                    dat.append($("<option />").val('SOA').text('SOA'));
                    dat.append($("<option />").val('CNAME').text('CNAME'));
                    dat.append($("<option />").val('PTR').text('PTR'));
                    dat.append($("<option />").val('SRV').text('SRV'));
                    dat.append($("<option />").val('DCHID').text('DCHID'));
                    dat.append($("<option />").val('CERT').text('CERT'));
                    dat.append($("<option />").val('DNSKEY').text('DNSKEY'));
                    dat.append($("<option />").val('RRSIG').text('RRSIG'));
                    dat.append($("<option />").val('CDNSKEY').text('CDNSKEY'));
                    dat.append($("<option />").val('NSEC').text('NSEC'));
                    dat.append($("<option />").val('TA').text('TA'));
                    dat.append($("<option />").val('IPSECKEY').text('IPSECKEY'));
                    dat.append($("<option />").val('KEY').text('KEY'));
                    dat.append($("<option />").val('DNAME').text('DNAME'));
                    dat.append($("<option />").val('AFSDB').text('AFSDB'));
                    dat.append($("<option />").val('APL').text('APL'));
                    dat.append($("<option />").val('CAA').text('CAA'));
                    dat.append($("<option />").val('CDS').text('CDS'));
                    dat.append($("<option />").val('CSYNC').text('CSYNC'));
                    dat.append($("<option />").val('DLV').text('DLV'));
                    dat.append($("<option />").val('DS').text('DS'));
                    dat.append($("<option />").val('EUI48').text('EUI48'));
                    dat.append($("<option />").val('EUI64').text('EUI64'));
                    dat.append($("<option />").val('HINFO').text('HINFO'));
                    dat.append($("<option />").val('HIP').text('HIP'));
                    dat.append($("<option />").val('HTTPS').text('HTTPS'));
                    dat.append($("<option />").val('KX').text('KX'));
                    dat.append($("<option />").val('NAPTR').text('NAPTR'));
                    dat.append($("<option />").val('LOC').text('LOC'));
                    dat.append($("<option />").val('OPENPGPKEY').text('OPENPGPKEY'));
                    dat.append($("<option />").val('NSEC3').text('NSEC3'));
                    dat.append($("<option />").val('NSEC3PARAM').text('NSEC3PARAM'));
                    dat.append($("<option />").val('RP').text('RP'));
                    dat.append($("<option />").val('SIG').text('SIG'));
                    dat.append($("<option />").val('SMIMEA').text('SMIMEA'));
                    dat.append($("<option />").val('SSHFP').text('SSHFP'));
                    dat.append($("<option />").val('SVCB').text('SVCB'));
                    dat.append($("<option />").val('TKEY').text('TKEY'));
                    dat.append($("<option />").val('TSIG').text('TSIG'));
                    dat.append($("<option />").val('TLSA').text('TLSA'));
                    dat.append($("<option />").val('URI').text('URI'));
                    dat.append($("<option />").val('ZONEMD').text('ZONEMD'));
                    dat.append($("<option />").val('AXFR').text('AXFR'));
                    dat.append($("<option />").val('IXFR').text('IXFR'));
                    dat.addClass("form-control");
                    dat.val(e.value);
                    dat.find('option[value="' + e.value + '"]').prop('selected', true);
                    return dat;
                } else {
                    var dat = $("<input>");
                    dat.addClass("form-control");
                    dat.attr("type", "text");
                    dat.val(e.value)
                    dat.after($("<span>")).text("");;
                    return dat;
                }
            }
        });

        reloadData();
    });
</script>
