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
$rndc_conf_path = escapeshellarg(BIND_LOCALBASE . "/etc/rndc.conf");
$rndc = "/usr/local/sbin/rndc -q -r -c {$rndc_conf_path} ";
$chkzone = '/usr/local/sbin/named-checkzone';

/**
 * Converts all special characters to html entities and adds <wbr>-tag as suffix to any point (.) to enable better line break in long host names.
 * @param string $string The input string.
 * @return string Returns the modified string.
 */
function htmlchars_wbr($string)
{
    $string = htmlspecialchars($string);
    $string = str_replace('.', '<wbr>.', $string);
    return $string;
}

if ($_POST) {
    $input_errors = array();
    $savemsg = array();
    $post = $_POST;
    $loadZone = false;

    foreach (['zone_editable', 'current_zone', 'zone_parsed'] as $key) {
        if ($_POST[$key]) {
            $post[$key] = unserialize(base64_decode($_POST[$key]));
        }
    }

    if ($post['zoneselect'] != $post['current_zone'] || $post["action"] == 'reload') {
        if ($post['zone_editable'] == "true") {
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

    if ($post["action"] == "download_zonefile") {
        if (!empty($selectedZone)) {
            $zonedb = ZoneParser::compilezone($zoneview, $zonename_reverse);
            header('Content-Type: text/plain');
            header('Content-Disposition: attachment; filename="' . $zonename . '_' . $zoneview . '.txt"');
            header('Cache-Control: no-cache');
            header('Content-Length: ' . strlen($zonedb));
            print($zonedb);
            exit;
        } else {
            $input_errors[] = gettext('No Zone selected.');
        }
    }

    if ($post["action"] == 'save') {
        $result = ZoneParser::validate_save_zonefile($post['zone_data'], $zonetype, $zoneview, $zonename_reverse);

        if ($result['success']) {
            $thaw = true;
            $loadZone = true;
            $savemsg[] = "Zonedata saved.";
            $savemsg[] = $result['message'];
        } else {
            $input_errors[] = gettext("Validation of new Zonefile failed.") . " Code {$result['result_code']}";
            $input_errors[] = $result['message'];
            $loadZone = false;
        }
    }

    if ($post["action"] == 'thaw' || $thaw == true) {
        exec("{$rndc} thaw " . escapeshellarg($zonename_reverse) . " IN " . escapeshellarg($zoneview) . ' 2>&1', $output, $resultCode);

        if ($resultCode == 0) {
            $post['zone_editable'] = "false";
            $savemsg[] = "Thaw successfull.";
            array_merge($savemsg, $output);
        } else {
            $input_errors[] = "RNDC THAW throwed an exception. Zone {$zonename_reverse} may still be frozen. Code {$resultCode}";
            array_merge($input_errors, $output);
        }
    }

    if ($post["action"] == 'thawall') {
        exec("{$rndc} thaw" . ' 2>&1', $output, $resultCode);

        if ($resultCode == 0) {
            $post['zone_editable'] = "false";
            $savemsg[] = "End-Edit (Thaw All) successfull.";
            array_merge($savemsg, $output);
        } else {
            $input_errors[] = "RNDC THAW throwed an exception. Code {$resultCode}";
            array_merge($input_errors, $output);
        }
    }

    if ($post["action"] == 'freeze') {
        exec("{$rndc} freeze " . escapeshellarg($zonename_reverse) . " IN " . escapeshellarg($zoneview) . ' 2>&1', $output, $resultCode);

        if ($resultCode == 0) {
            $loadZone = true;
            $post['zone_editable'] = "true";

            $savemsg[] = "Zone frozen. Don't forget to END EDIT before leaving.";
            array_merge($savemsg, $output);
        } else {
            $input_errors[] = "RNDC Freeze throwed an exception. Code {$resultCode}";
            array_merge($input_errors, $output);
        }
    }

    if ($loadZone === true) {
        try {
            $post['zone_data'] = ZoneParser::compilezone($zoneview, $zonename_reverse);
            $post['zone_parsed'] = ZoneParser::parse_rndc_zone_dump($post['zone_data'], $zonename_reverse, false);
            $savemsg[] = gettext("Zone Data loaded.");
        } catch (Exception $e) {
            $post['zone_data'] = '';
            $post['zone_editable'] = "false";
            $input_errors[] = $e->getMessage();
        }
    }

    if ($post['action'] == 'db_to_config') {

    }
}

$pgtitle = array(gettext("Status"), gettext("Edit zone"));
$shortcut_section = "bind";

include("head.inc");

// $tab_array = array();
// $tab_array[] = array(gettext("Show all Zones"), false, "/packages/binddump/binddump.php");
// $tab_array[] = array(gettext("Edit RAW Zone File"), true, "/packages/binddump/zoneEdit.php");
// display_top_tabs($tab_array);

$zonelist = [];
$zonelist_not_master = [];
foreach (ZoneParser::get_zonelist() as $zone) {
    $zonekey = $zone['view'] . '__' . $zone['name'] . '__' . ZoneParser::reverse_zonename($zone) . '__' . $zone['type'];
    if ($zone['type'] == 'master') {
        $zonelist[$zonekey] = ZoneParser::reverse_zonename($zone) . '  (' . $zone['view'] . ')';
    } else {
        $zonelist_not_master[$zonekey] = ZoneParser::reverse_zonename($zone) . '  (' . $zone['view'] . ')';
    }
}

if ($input_errors) {
    print_input_errors($input_errors);
}

if (!empty($savemsg)) {
    print_info_box(implode("\n", $savemsg), 'success');
}

?>

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
                    <select class="form-control" name="zoneselect" onchange="this.form.submit()">
                        <optgroup label="Master Zones">
                            <? foreach ($zonelist as $key => $value) { ?>
                                <option <? if ($key == $post['current_zone']) {
                                    print('selected="selected"');
                                } ?> value="<?= $key ?>"><?= $value ?>
                                </option>
                            <? } ?>
                        </optgroup>
                        <optgroup label="Other Zones (not Editable)">
                            <? foreach ($zonelist_not_master as $key => $value) { ?>
                                <option <? if ($key == $post['current_zone']) {
                                    print('selected="selected"');
                                } ?> value="<?= $key ?>"><?= $value ?>
                                </option>
                            <? } ?>
                        </optgroup>
                        <optgroup label="Automatically created Zones">
                        </optgroup>
                    </select>
                </div>
            </div>

            <div class="form-group">
                <div class="col-sm-2"></div>
                <div class="col-sm-10 help-block">
                    <?= gettext('While in Edit-Mode, DDNS Updates are disabled to prevent inconsistency.') ?>
                </div>
            </div>
        </div>
    </div>

    <ul style="margin-bottom:0px;" class="nav nav-tabs">
        <li class="active">
            <a data-toggle="tab" href="#tab1">RAW Zonefile</a>
        </li>
        <li>
            <a data-toggle="tab" href="#tab2">Table</a>
        </li>
    </ul>

    <div class="tab-content">
        <div id="tab1" class="tab-pane fade in active">
            <div class="panel panel-default">
                <div class="panel-body">
                    <div class="form-group">
                        <div class="col-sm-12 btn-group">
                            <button class="btn btn-default" type="submit" value="freeze" name="action">
                                <i class="fa fa-edit icon-embed-btn"></i>
                                <?= gettext('Start edit') ?>
                            </button>
                            <button class="btn btn-success" type="submit" value="save" name="action">
                                <i class="fa fa-download icon-embed-btn"></i>
                                <?= gettext('Save changes') ?>
                            </button>
                            <button class="btn btn-danger" type="submit" value="thaw" name="action">
                                <?= gettext('Cancel edit') ?>
                            </button>
                            <button class="btn btn-danger" type="submit" value="thawall" name="action">
                                <?= gettext('Cancel edit (All Zones)') ?>
                            </button>
                            <button class="btn btn-info" type="submit" value="download_zonefile" name="action">
                                <i class="fa fa-download icon-embed-btn"></i>
                                <?= gettext('Download Zone DB') ?>
                            </button>
                        </div>
                    </div>
                    <div class="form-group">
                        <div class="col-sm-12 ">
                            <textarea rows="15" class="form-control" name="zone_data" id="zone_data" wrap="off" <? if ($post['zone_editable'] !== 'true') {
                                print('readonly="readonly"');
                            } ?>><?= $post['zone_data'] ?></textarea>
                        </div>
                    </div>
                    <div class="form-group">
                        <div class="col-sm-12 btn-group">
                            <button class="btn btn-success" type="submit" value="save" name="action">
                                <?= gettext('Save changes') ?>
                            </button>
                        </div>
                    </div>
                </div>
            </div>
        </div>
        <div id="tab2" class="tab-pane fade">
            <? if (!empty($post['zone_parsed'])) {
                $skip = ['name_part1', 'name_part2', 'index', 'class', 'name', 'type'];
                ?>
                <div class="panel panel-default">
                    <div class="form-group">
                        <div class="col-sm-12 ">
                            <input id="showAdvanced" type="checkbox">Show Advanced Values.</input>
                        </div>
                    </div>

                    <div class="form-group">
                        <div class="table-responsive">
                            <table id="zonerecordlist" class="table table-striped table-condensed sortable-theme-bootstrap"
                                data-sortable>
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
                                    <? foreach ($post['zone_parsed'] as $record) { ?>
                                        <tr data-id="<?= $record['_id'] ?>">
                                            <td><i data-clipboard="<?= rtrim($record['name'], '.') ?>"
                                                    class="icon-pointer fa fa-clipboard"
                                                    title="<?= gettext('to Clipboard') ?>"></i>
                                                <span>
                                                    <?= htmlchars_wbr($record['name_part1']) ?>
                                                </span><wbr><span class="text-success">
                                                    <?= htmlchars_wbr($record['name_part2']) ?>
                                                </span>
                                            </td>
                                            <td>
                                                <?= htmlspecialchars($record['type']) ?>
                                            </td>
                                            <td>
                                                <?
                                                $sortKeys = array_unique(array_merge($record['_required'], array_keys($record)));
                                                foreach ($sortKeys as $key) {
                                                    if (!str_starts_with($key, '_') && !in_array($key, $skip)) {
                                                        $icon = '';
                                                        $requiredKey = $record['_required'] && in_array($key, $record['_required']);
                                                        $val = $record[$key];
                                                        $rval = rtrim($val, '.');

                                                        if ($requiredKey) {
                                                            $textclass = 'text-warning';
                                                        } else {
                                                            $textclass = 'text-success';
                                                        }

                                                        switch ($key) {
                                                            case 'host':
                                                            case 'ptr':
                                                            case 'mname':
                                                            case 'nameserver':
                                                                if (ZoneParser::record_exists_by_name($post['zone_parsed'], $val)) {
                                                                    $icon = '<i class="fa fa-check"></i>';
                                                                }
                                                            default:
                                                        }

                                                        print('<div ' . (!$requiredKey ? 'style="display:none" data-isadvancedoption="true"' : '') .
                                                            '><i data-clipboard="' . $rval . '" class="icon-pointer fa fa-clipboard" title="' . gettext("to Clipboard") . '"></i>&nbsp;<span class="text-uppercase ' . $textclass . '">' . htmlspecialchars($key) . ': </span>' .
                                                            htmlchars_wbr($val) . $icon . '</div>');
                                                    }
                                                }
                                                ?>
                                            </td>
                                            <td>
                                                <?= $record['_inconfig'] ?>
                                            </td>
                                            <td></td>
                                        </tr>
                                    <? } ?>
                                </tbody>
                            </table>
                        </div>
                    </div>

                    <div class="form-group">
                        <div class="col-sm-12">
                            <button class="btn btn-primary" type="button" value="add_record" name='action'><i
                                    class="fa fa-plus icon-embed-btn"> </i>
                                <?= gettext('Add') ?>
                            </button>
                        </div>
                    </div>
                </div>
            <? } ?>
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
                    <input class="btn btn-primary" type="submit" value="close" data-dismiss="modal">
                </div>
            </div>
        </div>
    </div>

    <input type='hidden' value="<?= base64_encode(serialize($post['zone_editable'])) ?>" name="zone_editable" />
    <input type='hidden' value="<?= base64_encode(serialize($post['current_zone'])) ?>" name="current_zone" />
    <input type='hidden' value="<?= base64_encode(serialize($post['zone_parsed'])) ?>" name="zone_parsed" />
</form>
<?
include('foot.inc');
?>
<script type="text/javascript">
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

    events.push(function () {
        $(window).on('beforeunload', function () {
            if ($("#zone_editable").val() == "true") {
                event.returnValue = `Discard changes, and leave Page? Remember it is required to end Edit Mode to re-enable DDNS Updates.`
            }
        })

        $("#showAdvanced").on("change", function () {
            if (this.checked) {
                $("[data-isadvancedoption=\"true\"]").show()
            } else {
                $("[data-isadvancedoption=\"true\"]").hide()
            }
        })

        $("a[data-clipboard]").on("click", async function (event) {
            event.preventDefault();

            try {
                $(this).attr("class", "fa fa-spinner fa-pulse")
                await navigator.clipboard.writeText($(this).attr("data-clipboard"))
                $(this).attr("class", "fa fa-check-circle")
            } catch (err) {
                $(this).attr("class", "fa fa-times-circle")
            }
        })
    })
</script>
