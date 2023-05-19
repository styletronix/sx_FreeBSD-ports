<?php
use binddump\ZoneParser;
/*
 * binddump.php
 */
require_once("guiconfig.inc");
require_once("config.inc");
require_once("/usr/local/pkg/binddump_zoneparser.inc");

define('BIND_LOCALBASE', '/usr/local');
define('CHROOT_LOCALBASE', '/var/etc/named');
$rndc_conf_path = BIND_LOCALBASE . "/etc/rndc.conf";
$rndc = "/usr/local/sbin/rndc -q -c {$rndc_conf_path}";


if ($_POST) {
    if (!empty($_POST['getList'])) {
        try {
            if (is_array($config['installedpackages']['bindzone'])) {
                $bindzone = $config['installedpackages']['bindzone']['config'];
            } else {
                $bindzone = array();
            }

            $selectedZone = explode('__', $_POST['zone']);
            $zoneview = $selectedZone[0];
            $zonename = $selectedZone[1];
            $zonename_reverse = $selectedZone[2];
            $zonetype = $selectedZone[3];

            $zone_data = ZoneParser::compilezone($zoneview, $zonename_reverse, $zonetype);
            $zone_data_parsed = ZoneParser::parse_rndc_zone_dump($zone_data, $zonename_reverse, false);
            foreach ($zone_data_parsed as &$data) {
                foreach ($bindzone as $zone) {
                    if ($zone['view'] == $zoneview && $zone['name'] == $zonename) {
                        $customzonerecords = base64_decode($zone['customzonerecords']);
                        $customzonerecords_parsed = ZoneParser::parse_rndc_zone_dump($customzonerecords, $zonename_reverse, false);
                        foreach ($customzonerecords_parsed as $customzonerecord) {
                            if (
                                $customzonerecord['name'] == $data['name']
                                && $customzonerecord['type'] == $data['type']
                                && $customzonerecord['rdata'] == $data['rdata']
                            ) {
                                $data['_inconfig'] = true;
                            }
                        }
                    }
                }
            }
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
            $post['zone_data'] = ZoneParser::compilezone($zoneview, $zonename_reverse);
        } catch (Exception $e) {
            $zrev = ZoneParser::re_reverse_zonename($zonename_reverse);
            $post['zone_data'] = $zrev; //'[error]';
            $input_errors[] = $e->getMessage();
            unset($_POST["save"]);
            $post['zone_editable'] = "false";
        }
    }

    // if (!empty($_POST["addToXML"])) {
    //     if (is_array($config['installedpackages']['bindzone'])) {
    //         $bindzone = $config['installedpackages']['bindzone']['config'];
    //     } else {
    //         $bindzone = array();
    //     }

    //     foreach ($bindzone as $zone) {
    //         if ($zone['view'] == $zoneview && $zone['name'] == $zonename) {
    //         }
    //     }
    // }

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
                $post['zone_data'] = ZoneParser::compilezone($zoneview, $zonename_reverse);
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
    $zone_data_parsed = ZoneParser::parse_rndc_zone_dump($post['zone_data'], $zonename_reverse, false);
} else {
    $zone_data_parsed = [];
}

$pgtitle = array(gettext("Status"), gettext("Edit zone"));
$shortcut_section = "bind";

include("head.inc");

$tab_array = array();
$tab_array[] = array(gettext("Show all Zones"), false, "/packages/binddump/binddump.php");
$tab_array[] = array(gettext("Edit RAW Zone File"), false, "/packages/binddump/zoneEdit.php");
$tab_array[] = array(gettext("Edit Zone File"), true, "/packages/binddump/zoneEditor.php");
display_top_tabs($tab_array);

$zonelist = [];
foreach (ZoneParser::get_zonelist() as $zone) {
    if ($zone['type'] == 'master') {
        $zonelist[$zone['view'] . '__' . $zone['name'] . '__' . ZoneParser::reverse_zonename($zone) . '__' . $zone['type']] = ZoneParser::reverse_zonename($zone) . '  (' . $zone['view'] . ')';
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
    }

    [data-status="added"] {
        font-weight: bold;
    }

    [data-status="unchanged"] {
    }

    .truncate {
        width: 250px;
        white-space: nowrap;
        overflow: hidden;
        text-overflow: ellipsis;
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
                    <select class="form-control" name="zoneselect" id="zoneselect">
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

            <div class="table-responsive">
                <table id="zonerecordlist"
                    class="table table-striped table-hover table-condensed sortable-theme-bootstrap" data-sortable>
                    <thead>
                        <tr>
                            <th>
                                <?= gettext("Name") ?>
                            </th>
                            <th>
                                <?= gettext("Type") ?>
                            </th>
                            <th>
                                <?= gettext("Values") ?>
                            </th>
                            <th>
                                <?= gettext("in config") ?>
                            </th>
                            <th>
                                <?= gettext("Action") ?>
                            </th>
                        </tr>
                    </thead>
                    <tbody>
                        <tr data-template-row="default" style="display: none;">
                            <td data-fieldname="name"></td>
                            <td data-fieldname="type"></td>
                            <td>
                                <div style="display: none;" data-fieldname-cell="type"><span class="text-uppercase text-success">Type: </span><span data-fieldname="type"></span></div>
                                <div style="display: none;" data-fieldname-cell="ttl">><span class="text-uppercase text-success">TTL: </span><span data-fieldname="ttl"></span></div>
                                <div style="display: none;" data-fieldname-cell="mname"><span class="text-uppercase text-success">mname: </span><span data-fieldname="mname"></span></div>
                                <div style="display: none;" data-fieldname-cell="serial"><span class="text-uppercase text-success">serial: </span><span data-fieldname="serial"></span></div>
                                <div style="display: none;" data-fieldname-cell="refresh"><span class="text-uppercase text-success">refresh: </span><span data-fieldname="refresh"></span></div>
                                <div style="display: none;" data-fieldname-cell="retry"><span class="text-uppercase text-success">retry: </span><span data-fieldname="retry"></span></div>
                                <div style="display: none;" data-fieldname-cell="expire"><span class="text-uppercase text-success">expire: </span><span data-fieldname="expire"></span></div>
                                <div style="display: none;" data-fieldname-cell="minimum"><span class="text-uppercase text-success">minimum: </span><span data-fieldname="minimum"></span></div>
                                <div style="display: none;" data-fieldname-cell="priority"><span class="text-uppercase text-success">Priority: </span><span data-fieldname="priority"></span></div>
                                <div style="display: none;" data-fieldname-cell="weight"><span class="text-uppercase text-success">Weight: </span><span data-fieldname="weight"></span></div>
                                <div style="display: none;" data-fieldname-cell="port"><span class="text-uppercase text-success">Port: </span><span data-fieldname="port"></span></div>
                                <div style="display: none;" data-fieldname-cell="host"><span class="text-uppercase text-success">Host: </span><span data-fieldname="host"></span></div>
                                <div style="display: none;" data-fieldname-cell="ipv4"><span class="text-uppercase text-success">IP: </span><span data-fieldname="ip"></span></div>
                                <div style="display: none;" data-fieldname-cell="ipv6"><span class="text-uppercase text-success">IP: </span><span data-fieldname="ip"></span></div>
                                <div style="display: none;" data-fieldname-cell="nameserver"><span class="text-uppercase text-success">Nameserver: </span><span data-fieldname="nameserver"></span></div>
                                <div style="display: none;" data-fieldname-cell="ptr"><span class="text-uppercase text-success">PTR: </span><span data-fieldname="ptr"></span></div>
                                <div style="display: none;" data-fieldname-cell="txt"><span class="text-uppercase text-success">TXT: </span><span data-fieldname="txt"></span></div>
                                <div style="display: none;" data-fieldname-cell="rdata" class="truncate" onclick="showtruncated(this)"><span class="text-uppercase">Raw Data: </span><span data-fieldname="rdata"></span></div>
                            </td>
                            <td data-fieldname="_inconfig"></td>
                        </tr>
                        <tr data-template-row="edit" style="display: none;">
                            <td data-fieldname="name"></td>
                            <td>
                               <select data-fieldname="type">
                                    <option value="A">A</option>
                                    <option value="AAAA">AAAA</option>
                                    <option value="TXT">TXT</option>
                                    <option value="SPF">SPF</option>
                                    <option value="MX">MX</option>
                                    <option value="NS">NS</option>
                                    <option value="SOA">SOA</option>
                                    <option value="CNAME">CNAME</option>
                                    <option value="PTR">PTR</option>
                                    <option value="SRV">SRV</option>
                                    <option value="DCHID">DCHID</option>
                                    <option value="CERT">CERT</option>
                                    <option value="DNSKEY">DNSKEY</option>
                                    <option value="RRSIG">RRSIG</option>
                                    <option value="CDNSKEY">CDNSKEY</option>
                                    <option value="NSEC">NSEC</option>
                                    <option value="TA">TA</option>
                                    <option value="IPSECKEY">IPSECKEY</option>
                                    <option value="KEY">KEY</option>
                                    <option value="DNAME">DNAME</option>
                                    <option value="AFSDB">AFSDB</option>
                                    <option value="APL">APL</option>
                                    <option value="CAA">CAA</option>
                                    <option value="CDS">CDS</option>
                                    <option value="CSYNC">CSYNC</option>
                                    <option value="DLV">DLV</option>
                                    <option value="DS">DS</option>
                                    <option value="EUI48">EUI48</option>
                                    <option value="EUI64">EUI64</option>
                                    <option value="HINFO">HINFO</option>
                                    <option value="HIP">HIP</option>
                                    <option value="HTTPS">HTTPS</option>
                                    <option value="KX">KX</option>
                                    <option value="NAPTR">NAPTR</option>
                                    <option value="LOC">LOC</option>
                                    <option value="OPENPGPKEY">OPENPGPKEY</option>
                                    <option value="NSEC3">NSEC3</option>
                                    <option value="NSEC3PARAM">NSEC3PARAM</option>
                                    <option value="RP">RP</option>
                                    <option value="SIG">SIG</option>
                                    <option value="SMIMEA">SMIMEA</option>
                                    <option value="SSHFP">SSHFP</option>
                                    <option value="SVCB">SVCB</option>
                                    <option value="TKEY">TKEY</option>
                                    <option value="TSIG">TSIG</option>
                                    <option value="TLSA">TLSA</option>
                                    <option value="ZONEMD">ZONEMD</option>
                                    <option value="URI">URI</option>
                                    <option value="AXFR">AXFR</option>
                                    <option IXFR="TLSA">IXFR</option>
                                </select>
                            </td>
                            <td>
                                <input data-fieldname="priority" type="number" />
                                <input data-fieldname="hostname" type="mailserver" />
                                <input data-fieldname="ip" type="text" />
                                <input data-fieldname="rdata" type="text" />
                            </td>
                            <td>
                                <input data-fieldname="_inconfig" type="checkbox" />
                            </td>
                        </tr>
                    </tbody>
                </table>
                <div class="col-sm-10 col-sm-offset-2">
                    <button class="btn btn-primary" type="button" value="<?= gettext('Save') ?>" name="save" id="save"
                        onclick="submitChanges();"><i class="fa fa-save icon-embed-btn"> </i>
                        <?= gettext('Save') ?>
                    </button>
                    <button class="btn btn-primary" type="button" value="<?= gettext('Add') ?>" id="btnNewRow"><i
                            class="fa fa-add icon-embed-btn"> </i>
                        <?= gettext('Add') ?>
                    </button>
                </div>
            </div>
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
                        <span aria-hidden="true">X</span>
                    </button>
                    <h3 class="modal-title">Please wait</h3>
                </div>
                <div class="modal-body">
                    <div class="form-group">
                        <label class="col-sm-8 control-label">
                            <span></span>
                        </label>
                        <div class="col-sm-10">
                            Please wait for the process to complete.<br><br>This dialog will auto-close when the update
                            is finished.<br><br><i
                                class="content fa fa-spinner fa-pulse fa-lg text-center text-info"></i>
                        </div>

                    </div>
                </div>
                <div class="modal-footer">
                    <input class="btn btn-primary" type="submit" value=" <?= gettext('Close') ?>" name="save" id="save"
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

    function showtruncated(e){
        $(e).closest(".truncate").removeClass("truncate");
    }

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

    function submitChanges() {
        showWait("<?= gettext('Saving Zone Data') ?> ");

        var data = table1.sxTable("getChangeSet");
        alert(JSON.stringify(data));

        //TODO: Save changes.....
        table1.sxTable("mergeChanges");
        hideWait();
    }

    function reloadData() {
        var zone = $('#zoneselect').find(":selected").val();
        showWait("<?= gettext('Loading new Zone Data') ?> ");

        table1.sxTable("clear");

        $.ajax({
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
        $('#zoneselect').change(function() {
            reloadData($(this).val());
        });

        $(window).on('beforeunload', function () {
            if (count(table1.sxTable("getChangeSet") > 0)){
                return "You have unsaved changes on this page. Do you want to leave this page and discard your changes?";
            }
        });

        table1 = $('#zonerecordlist').sxTable();
    });
</script>
