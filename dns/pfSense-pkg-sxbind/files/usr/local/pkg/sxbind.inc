<?php
use sxbind\ZoneParser;

/*
 * sxbind.inc
 *

 * All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
require_once("globals.inc");
require_once("config.inc");
require_once("util.inc");
require_once("pfsense-utils.inc");
require_once("pkg-utils.inc");
require_once("service-utils.inc");
require_once("sxbind_zoneparser.inc");

if (!function_exists("filter_configure")) {
	require_once("filter.inc");
}
require_once("notices.inc");
require_once("xmlrpc_client.inc");

define('BIND_LOCALBASE', '/usr/local');
define('CHROOT_LOCALBASE', '/var/etc/named');

function sxbind_zone_validate(&$post, &$input_errors)
{
	if ($post['type'] == "master" and $post['serial'] == "") {
		$post['serial'] = (date("U") + 1000000000);
	}

	if (array_key_exists("mail", $_POST)) {
		$_POST['mail'] = preg_replace("/@/", ".", $post['mail']);
	}

	if (isset($_POST['rpz']) && ($_POST['rpz'] == "on")) {
		if (($_POST['type'] !== "master") && ($_POST['type'] !== "slave")) {
			$input_errors[] = "Only a master or slave zone can be used in a response policy";
		}
	}
	switch ($_POST['type']) {
		case 'redirect':
			$_POST['tll'] = 300;
			$_POST['refresh'] = 0;
			$_POST['serial'] = 0;
			$_POST['retry'] = 0;
			$_POST['expire'] = 0;
			$_POST['minimum'] = 0;
			if ($_POST['mail'] == '') {
				$input_errors[] = "The field 'Mail Admin Zone' is required for {$_POST['type']} zones.";
			}
		// Intentionally fall through to 'master' zone case
		case 'master':
			if ($_POST['nameserver'] == '') {
				$input_errors[] = "The field 'Name server' is required for {$_POST['type']} zones.";
			}
			// for ($i = 0; $i < count($_POST); $i++) {
			// 	if (array_key_exists("hostname$i", $_POST)) {
			// 		if ($_POST['reverso'] == "on") {
			// 			$_POST["hostvalue$i"] = "";
			// 			if (!preg_match("/(PTR|NS)/", $_POST["hosttype$i"])) {
			// 				$input_errors[] = 'On reverse zones, valid record types are NS or PTR';
			// 			}
			// 		}
			// 		if (!preg_match("/(MX|NS)/", $_POST["hosttype$i"]) && ($_POST["hostname$i"] == "")) {
			// 			$input_errors[] = 'Record cannot be empty for ' . $_POST["hosttype$i"] . ' type ';
			// 		}
			// 		if ($_POST["hosttype$i"] == "MX" && ($_POST["hostvalue$i"] == "")) {
			// 			$_POST["hostvalue$i"] = "10";
			// 		}
			// 		if ($_POST["hosttype$i"] != "MX" && ($_POST["hostvalue$i"] != "")) {
			// 			$_POST["hostvalue$i"] = "";
			// 		}
			// 		if ($_POST["hostdst$i"] == "") {
			// 			$input_errors[] = 'Alias or IP address cannot be empty.';
			// 		}
			// 	}
			// }
			break;
		case 'slave':
			if ($_POST['slaveip'] == "") {
				$input_errors[] = 'The field \'Master Zone IP\' is required for slave zones.';
			}
			break;
		case 'forward':
			if ($_POST['forwarders'] == "") {
				$input_errors[] = 'The field \'Forwarders\' is required for forward zones.';
			}
			break;
		default:
			break;
	}
}

if (!function_exists('pf_version')) {
	function pf_version()
	{
		return substr(trim(file_get_contents("/etc/version")), 0, 5);
	}
}

/* unescape zone record. 
TODO: Maybe there are additional escape rules? */
function sxbind_escape_dns_string($val)
{
	$search = ['\\', '"', ';'];
	$replace = ['\\\\', '\\"', '\\;'];
	return '"' . str_replace($search, $replace, $val) . '"';
}
function get_top_tabs()
{
	$active = basename($_SERVER['PHP_SELF']);
	if ($active == 'pkg_edit.php') {
		$active = $_REQUEST['xml'];
	}

	$tab_array = array();
	$tab_array[] = array(gettext("Settings"), (($active == 'sxbind.xml') ? true : false), "/pkg_edit.php?xml=sxbind.xml");
	$tab_array[] = array(gettext("ACLs"), (($active == 'sxbind_acls.xml') ? true : false), "/pkg_edit.php?xml=sxbind_acls.xml");
	$tab_array[] = array(gettext("Views"), (($active == 'sxbind_views.xml') ? true : false), "/pkg_edit.php?xml=sxbind_views.xml");
	$tab_array[] = array(gettext("Zones"), (($active == 'sxbind_zones.xml') ? true : false), "/pkg_edit.php?xml=sxbind_zones.xml");
	$tab_array[] = array(gettext("Tools"), (($active == 'sxbind_tools.php') ? true : false), "/sxbind_tools.php");
	$tab_array[] = array(gettext("Edit Zone DB"), (($active == 'sxbind_zoneEdit.php') ? true : false), "/sxbind_zoneEdit.php");
	$tab_array[] = array(gettext("Sync"), (($active == 'sxbind_sync.xml') ? true : false), "/pkg_edit.php?xml=sxbind_sync.xml");
	display_top_tabs($tab_array);
}
function sxbind_set_serial_zoneconf($zone_conf, $new_zoneserial)
{
	// write new serial to zone_conf.
	$zone_conf_new = '';
	$serialfound = false;
	foreach (explode("\n", $zone_conf) as $zone_conf_row) {
		if ($serialfound == false && str_ends_with(trim($zone_conf_row), "; serial")) {
			$zone_conf_new .= "\t\t{$new_zoneserial} ; serial\n";
			$serialfound = true;
		} else {
			$zone_conf_new .= $zone_conf_row . "\n";
		}
	}
	if ($serialfound) {
		$zone_conf = $zone_conf_new;
	} else {
		log_error('[sxbind] WARNING: Serial in zone_conf not found!');
	}
	return $zone_conf;
}
function file_put_contents_if_changed($file, $content, &$changed)
{
	if (!file_exists($file) || file_get_contents($file) != $content) {
		file_put_contents($file, $content);
		$changed = true;
		return true;
	}
	return false;
}
// function sxbind_parse_config($str)
// {

// 	//$text = file_get_contents('/usr/local/etc/namedb/rndc.conf');
// // /"(?:\\\\.|[^\\\\"])*"|(?:\\\\.*\n)|(?:\#.*\n)|(?:\/\/.*\n)|\S+/

// 	//print_r($matches);



// 	// get quoted word list
// 	//$re = '/(?\'val2\'"(?:\\\\\\\\.|[^\\\\\\\\"])*")|(?\'comstart\'\/\*)|(?\'comend\'\*\/)|(?\'comment\'(?:(?:\/\/)|#).*\n|\/\*.*\*\/)|(?\'start\'{)|(?\'end\'})|(?\'val1\'[^\s;]+)|(?\'nextval\';)/';
// 	// preg_match_all('/(?\'val2\'\"(?:\\\\\\\\.|[^\\\\\\\\\"])*\")|(?\'comstart\'\\/\\*)|(?\'comend\'\\*\\/)|(?\'comment\'(?:(?:\\/\\/)|#).*\\n|\\/\\*.*\\*\\/)|(?\'start\'{)|(?\'end\'})|(?\'val1\'[^\\s;]+)|(?\'nextval\';)/', $text, $matches);
// 	//preg_match_all($re, $str, $matches, PREG_SET_ORDER, 0);

// 	//$re = '/"(?:\\\\\\\\.|[^\\\\\\\\"])*"|[;{}]|\/\/|\/\*|#|\*\/|[^\n{; \t]+/';

// 	$result = array();
// 	$blocks = array();
// 	$block_depth = 0;
// 	$block_id = 0;
// 	$in_comment_block = false;

// 	$block_chain = array();
// 	$blocks[$block_id] = array();
// 	$cur = &$blocks[$block_id];
// 	$block_chain[$block_depth] = &$cur;

// 	$lines = explode("\n", $str);

// 	foreach ($lines as $line) {
// 		preg_match_all($re, $line, $matches, PREG_SET_ORDER, 0);
// 		foreach ($matches as $words) {
// 			$word = $words[0];
// 			if ($in_comment_block && $word == '*/') {
// 				// end of comment block
// 				$in_comment_block = false;
// 				continue;
// 			}

// 			if ($word == '//' || $word == '#') {
// 				// skip line with comments
// 				continue 2;
// 			}

// 			if ($word == '/*') {
// 				// start of comment block
// 				$in_comment_block = true;
// 				continue;
// 			}

// 			if ($in_comment_block) {
// 				continue;
// 			}

// 			if ($word == '{') {
// 				// start of block
// 				$block_depth++;
// 				$block_id++;
// 				$blocks[$block_id] = array();
// 				$block_chain[$block_depth] = &$blocks[$block_id];
// 				$block_chain[$block_depth - 1][] = &$blocks[$block_id];
// 				continue;
// 			}

// 			if ($word == '}') {
// 				// end of block
// 				$block_depth--;
// 				$cur = &$block_chain[$block_depth];
// 				continue;
// 			}

// 			if ($word == ';') {
// 				// end of value
// 				$block_id++;
// 				$blocks[$block_id] = array();
// 				$cur[] = &$blocks[$block_id];
// 				continue;
// 			}


// 			if (count_chars($word) > 0) {
// 				$blocks[$block_id][] = $word;
// 			}
// 		}
// 	}
// 	//print_r($blocks);
// 	return $block_chain[0];
// }

// Array
// (
//     [0] => Array
//         (
//             [0] => key
//             [1] => "rndc-key"
//             [2] => {
//             [3] => algorithm
//             [4] => hmac-sha256;
//             [5] => secret
//             [6] => "Gfqkdz7fAC4zJD/As/dD8GlGY1HOQm3ZXbBpqYR4oKI="
//             [7] => ;
//             [8] => };
//             [9] => options
//             [10] => {
//             [11] => default-key
//             [12] => "rndc-key"
//             [13] => ;
//             [14] => default-server
//             [15] => 127.0.0.1;
//             [16] => default-port
//             [17] => 8953;
//             [18] => };
//         )

// )
//}
// function sxbind_set_config_value($data, $config_path, $value)
// {
// 	$result = [];

// 	$index = 0;
// 	$result_index = 0;

// 	foreach (explode("\n", $data) as $line) {
// 		$remaining_str = $line;
// 		$result[$result_index] = '';
// 		$trimmed = ltrim($remaining_str);

// 		while (count_chars($trimmed) > 0) {
// 			if (str_starts_with($trimmed, $config_path[$index])) {
// 				$opt_name_length = count_chars($config_path[$index]) + count_chars($remaining_str) - count_chars($trimmed);
// 				$result[$result_index] .= substr($remaining_str, 0, $opt_name_length);
// 				$index++;

// 				if ($index == count($config_path)) {
// 					$result[$result_index] .= substr($remaining_str, 0, $opt_name_length);

// 				} else {
// 					$result[$result_index] .= ' ';
// 				}
// 			}
// 		}

// 		if (str_ends_with($remaining_str, '}')) {
// 			$index--;
// 		}


// 		$result[$result_index] .= $remaining_str;
// 		$result_index++;
// 	}

// 	return implode("\n", $result);
// }

function sxbind_rndc_config_setval($str, $name, $value, &$changed)
{
	switch ($name) {
		case 'controls/inet/port':
			$re = '/^\s?(?:controls\s*\{[\s\S]*?inet.*?port\s)\K(.*)(?=[\s\S]+)/m';
			break;
		case 'options/default-port':
			$re = '/^\s?(?:options\s*\{[\s\S]*?default-port\s)\K([^;\s]*)(?=[\s\S]+)/m';
			break;
		default:
			throw new Exception('Option not supported');
	}
	$count = 0;
	preg_match($re, $str, $matches);
	if ($matches[0] != $value) {
		log_error('[SXBIND] INFO - change ' . $name . ' from ' . $matches[0] . ' to ' . $value);
		$str = preg_replace($re, $value, $str, 1, $count);
		if ($count == 0) {
			log_error('[SXBIND] ERROR - change of ' . $name . ' failed.');
			return null;
		}
		$changed = true;
	}

	return $str;
}
/**
 * Change or add TSIG Key to rndc.conf or named.conf.
 * @param mixed $str	The current config file content
 * @param mixed $key_string	The new Key as produced by tsig-keygen.
 * @return array|string|null	Returns null if failed or the new config file content.
 */
function sxbind_change_or_add_key($str, $key_string, &$changed)
{
	$re = '/key\s*\"(.*)\"\s*\{[\s\S]*?\}\s*;?/m';
	preg_match($re, $key_string, $matches);
	if (empty($matches)) {
		$error = "[sxbind] ERROR - tsig-key was not found in parameters";
		log_error($error);
		return null;
	}

	$key = $matches[0];
	$key_name = $matches[1];
	if (empty($key) || empty($key_name)) {
		$error = "[sxbind] ERROR - tsig-key was not found in parameters";
		log_error($error);
		return null;
	}

	$re = '/key\s*\"' . preg_quote($key_name) . '\"\s*\{[\s\S]*?\}\s*?;?/m';
	preg_match($re, $str, $matches);
	if ($matches[0] != $key) {
		$error = "[sxbind] INFO - tsig-key has changed";
		log_error($error);
		$changed = true;
		$str = preg_replace($re, $key, $str, 1, $count);
		if ($count == 0) {
			$error = "[sxbind] INFO - TSIG Key was not found. Key will be added.";
			log_error($error);
			$str .= "\n\n" . $key . "\n";
		}
	}
	return $str;
}
function sxbind_get_key($str, $key_name)
{
	$re = '/key\s*\"' . preg_quote($key_name) . '\"\s*\{[\s\S]*?\}\s*;?/m';
	preg_match($re, $str, $matches);
	if (empty($matches)) {
		return null;
	} else {
		return $matches[0];
	}
}
/**
 *Removes a TSIG-Key from rndc.conf or named.conf.
 * @param mixed $str	The current config file content
 * @param mixed $key_name	The name of the Key to remove without quotes.
 * @return array|string|null	Returns null if failed or the new config file content.
 */
function sxbind_remove_key($str, $key_name, &$changed)
{
	$re = '/^\s?key \"' . preg_quote($key_name) . '\" \{[\s\S]+?\}\s?;?/m';
	$ret = preg_replace($re, '', $str, 1, $count);
	if ($count == 0) {
		return null;
	} else {
		$changed = true;
		return $ret;
	}
}
function sxbind_create_rndc_conf_file($force = false, &$new_rndc_key = null, &$changed = false)
{
	global $config;
	$bind = $config['installedpackages'][ZoneParser::CONFIGPREFIX . 'bind']['config'][0];
	$rndc_file = BIND_LOCALBASE . '/etc/rndc.conf';
	$rndc_port = $bind['controlport'] == "" ? "8953" : $bind['controlport'];

	if (file_exists($rndc_file) && $force == false) {
		$rndc_conf = file_get_contents($rndc_file);

		$rndc_conf = sxbind_rndc_config_setval($rndc_conf, 'options/default-port', $rndc_port, $changed);
		if ($rndc_conf && $new_rndc_key) {
			$rndc_conf = sxbind_change_or_add_key($rndc_conf, $new_rndc_key, $changed);
		} else {

			$new_rndc_key = sxbind_get_key($rndc_conf, 'rndc-key');
			if (!$new_rndc_key) {
				$error = "[sxbind] INFO - rndc-key will be created because it is missing.";
				log_error($error);
				$new_rndc_key = ZoneParser::create_tsig_key('rndc-key');
				$rndc_conf = sxbind_change_or_add_key($rndc_conf, $new_rndc_key, $changed);
			}
		}
	}

	if ($force || !file_exists($rndc_file) || !$rndc_conf) {
		$error = "[sxbind] INFO - rndc.conf will be created because it is missing or the creation was forced.";
		log_error($error);

		$rndc_conf = '';
		$changed = true;

		if ($new_rndc_key == null) {
			$new_rndc_key = ZoneParser::create_tsig_key('rndc-key');
		}
		$rndc_conf .= "\n" . $new_rndc_key . "\n\n";

		$rndc_conf .= <<<EOD
options {
	default-key "rndc-key";
	default-server localhost;
	default-port {$rndc_port};
};
EOD;

	}

	if ($rndc_conf) {
		file_put_contents_if_changed($rndc_file, $rndc_conf, $changed);
		file_put_contents_if_changed(CHROOT_LOCALBASE . '/etc/namedb/rndc.conf', $rndc_conf, $changed);
		@symlink($rndc_file, BIND_LOCALBASE . "/etc/namedb/rndc.conf");
	}

	return $rndc_conf;
}
function sxbind_create_bind_conf(&$bind, $rndc_key, &$changed)
{
	global $config;
	$rndc_port = $bind['controlport'] == "" ? "8953" : $bind['controlport'];
	$bind_dnssec_validation = $bind['bind_dnssec_validation'];
	$bind_forwarder = $bind['bind_forwarder'];
	$forwarder_ips = $bind['bind_forwarder_ips'];
	$ram_limit = $bind['bind_ram_limit'] ? $bind['bind_ram_limit'] : "256M";
	$hide_version = $bind['bind_hide_version'];
	$bind_notify = $bind['bind_notify'];
	$custom_options = base64_decode($bind['bind_custom_options']);
	$bind_global_settings = base64_decode($bind['bind_global_settings']);
	$bind_logging = $bind['bind_logging'];

	$bind_conf = <<<EOD
# Bind pfsense configuration
# Do not edit this file!

{$rndc_key}

controls {
	inet 127.0.0.1 port {$rndc_port}
		allow { 127.0.0.1; ::1; } keys { "rndc-key"; };
};
EOD;
	$bind_conf .= "{$bind_global_settings}\n";
	// Curly braces in the following <<<EOD are PHP {$variable}, not named.conf text { value; }
	$bind_conf .= <<<EOD

options {
	directory "/etc/namedb";
	pid-file "/var/run/named/pid";
	statistics-file "/var/log/named.stats";
	max-cache-size {$ram_limit};

EOD;

	$bind_conf .= "\tdnssec-validation ";
	if ($bind_dnssec_validation == 'on') {
		$bind_conf .= "yes";
	} elseif ($bind_dnssec_validation == 'off') {
		$bind_conf .= "no";
	} else {
		$bind_conf .= "auto";
	}
	$bind_conf .= ";\n\n";

	// Check response rate limit option
	//https://kb.isc.org/article/AA-01000/0/A-Quick-Introduction-to-Response-Rate-Limiting.html
	//http://ss.vix.su/~vjs/rl-arm.html
	if ($bind['rate_enabled'] == "on") {
		$rate_limit = $bind['rate_limit'] ? $bind['rate_limit'] : "15";
		$log_only = $bind['log_only'] == "no" ? "no" : "yes";
		// Curly braces in the following <<<EOD are PHP {$variable}, not named.conf text { value; }
		$bind_conf .= <<<EOD
	rate-limit {
		responses-per-second {$rate_limit};
		log-only {$log_only};
	};

EOD;
	}
	// Check IPs to listen on
	if (preg_match("/All/", $bind['listenon'])) {
		$bind_listenonv6 = "any;";
		$bind_listenon = "any;";
	} else {
		$bind_listenonv6 = "";
		$bind_listenon = "";
		foreach (explode(',', $bind['listenon']) as $listenon) {
			if (is_ipaddrv6($listenon)) {
				$bind_listenonv6 .= $listenon . "; ";
			} elseif (is_ipaddr($listenon)) {
				$bind_listenon .= $listenon . "; ";
			} else {
				$listenon = pfSense_get_interface_addresses(convert_friendly_interface_to_real_interface_name($listenon));
				if (is_ipaddr($listenon['ipaddr'])) {
					$bind_listenon .= $listenon['ipaddr'] . "; ";
				}
				if (is_ipaddrv6($listenon['ipaddr6'])) {
					$bind_listenonv6 .= $listenon['ipaddr6'] . "; ";
				}
			}
		}
	}
	$bind_listenonv6 = $bind_listenonv6 == "" ? "none;" : $bind_listenonv6;
	$bind_listenon = $bind_listenon == "" ? "none;" : $bind_listenon;
	$bind_listenport = $bind['listenport'] == "" ? "53" : $bind['listenport'];
	if (array_key_exists("ipv6allow", $config['system'])) {
		$bind_conf .= "\tlisten-on-v6 port {$bind_listenport} { {$bind_listenonv6} };\n";
	}
	$bind_conf .= "\tlisten-on port {$bind_listenport} { {$bind_listenon} };\n";

	// Forwarder config
	if ($bind_forwarder == 'on') {
		if (!empty($forwarder_ips) && !strpos($forwarder_ips, ';', -1)) {
			$forwarder_ips .= ';';
		}
		$bind_conf .= "\tforwarders { {$forwarder_ips} };\n";
	}
	if ($bind_notify == 'on') {
		$bind_conf .= "\tnotify yes;\n";
	}
	if ($hide_version == 'on') {
		$bind_conf .= "\tversion none;\n";
	}

	$bind_conf .= preg_replace("/^/m", "\t", $custom_options);
	$bind_conf .= "\n};\n\n";

	if ($bind_logging == 'on') {
		// See https://redmine.pfsense.org/issues/5524
		if (pf_version() < "2.3.3") {
			// Check if bind is included on syslog
			$syslog_files = array("/etc/inc/system.inc", "/var/etc/syslog.conf");
			$restart_syslog = false;
			foreach ($syslog_files as $syslog_file) {
				$syslog_file_data = file_get_contents($syslog_file);
				if (!preg_match("/dnsmasq,named,filterdns/", $syslog_file_data) || !preg_match("/'dnsmasq','named','filterdns'/", $syslog_file_data)) {
					$syslog_file_data = preg_replace("/dnsmasq,filterdns/", "dnsmasq,named,filterdns", $syslog_file_data);
					$syslog_file_data = preg_replace("/'dnsmasq','filterdns'/", "'dnsmasq','named','filterdns'", $syslog_file_data);
					if (file_put_contents_if_changed($syslog_file, $syslog_file_data, $changed)) {
						$restart_syslog = true;
					}
				}
			}
			if ($restart_syslog > 0) {
				system("/usr/bin/killall -HUP syslogd");
			}
		}
		$log_categories = explode(",", $bind['log_options']);
		$log_severity = $bind['log_severity'] ? $bind['log_severity'] : 'default';
		if (sizeof($log_categories) > 0 && ($log_categories[0] != "")) {
			// Curly braces in the following <<<EOD are PHP {$variable}, not named.conf text { value; }
			$bind_conf .= <<<EOD

logging {
	channel custom {
		syslog daemon;
		print-time no;
		print-severity yes;
		print-category yes;
		severity {$log_severity};
		};

EOD;
			foreach ($log_categories as $category) {
				$bind_conf .= "\tcategory {$category} { custom; };\n";
			}
			$bind_conf .= "};";
		}
	} else {
		$bind_conf .= "logging { category default { null; }; };";
	}
	$bind_conf .= "\n\n";

	return $bind_conf;
}
function sxbind_append_acl_to_bind_conf(&$conf, &$bindacls)
{
	for ($i = 0; $i < sizeof($bindacls); $i++) {
		$aclname = $bindacls[$i]['name'];
		$aclhost = $bindacls[$i]['row'];
		if (($aclname != "none") && ($aclname != "any") && ($aclname != "localhost") && ($aclname != "localnets")) {
			$conf .= "acl \"{$aclname}\" {\n";
			for ($u = 0; $u < sizeof($aclhost); $u++) {
				$aclhostvalue = $aclhost[$u]['value'];
				$conf .= "\t{$aclhostvalue};\n";
			}
			$conf .= "};\n\n";
		}
	}
}
function sxbind_append_views_to_bind_conf(&$bind_conf, &$bindview, $bindzone)
{
	global $config;

	for ($i = 0; $i < sizeof($bindview); $i++) {
		$views = $config['installedpackages'][ZoneParser::CONFIGPREFIX . 'bindviews']['config'][$i];
		$viewname = $views['name'];
		$viewrecursion = $views['recursion'];
		if ($views['match-clients'] == '') {
			$viewmatchclients = "none";
		} else {
			$viewmatchclients = str_replace(',', '; ', $views['match-clients']);
		}
		if ($views['allow-recursion'] == '') {
			$viewallowrecursion = "none";
		} else {
			$viewallowrecursion = str_replace(',', '; ', $views['allow-recursion']);
		}
		$viewcustomoptions = base64_decode($views['bind_custom_options']);

		$bind_conf .= "view \"{$viewname}\" { \n";
		$bind_conf .= "\trecursion {$viewrecursion};\n";
		$bind_conf .= "\tmatch-clients { $viewmatchclients; };\n";
		$bind_conf .= "\tallow-recursion { $viewallowrecursion; };\n";

		$rpz_zones = array();
		foreach ($bindzone as $zone) {
			if (
				isset($zone['rpz']) && ($zone['rpz'] == "on") &&
				($zone['disabled'] != "on") && ($zone['view'] == $viewname)
			) {
				$rpz_zones[] = reverse_zonename($zone['name'], $zone['reverso'], $zone['reversv6o']);
			}
		}
		if (!empty($rpz_zones)) {
			$bind_conf .= "\n\tresponse-policy {\n";
			foreach ($rpz_zones as $rpz) {
				$bind_conf .= "\t\tzone \"{$rpz}\";\n";
			}
			$bind_conf .= "\t};\n";
		}

		if (!empty($viewcustomoptions)) {
			$bind_conf .= preg_replace("/^/m", "\t", $viewcustomoptions) . "\n";
		}
		$bind_conf .= "\n";

		// Add Zones in View
		$custom_root_zone = false;
		for ($x = 0; $x < sizeof($bindzone); $x++) {
			$zone = $bindzone[$x];
			if ($zone['disabled'] == "on") {
				continue;
			}
			$zonename = $zone['name'];
			if ($zonename == ".") {
				$custom_root_zone = true;
			}
			$zonetype = $zone['type'];
			$zoneviewlist = explode(',', $zone['view']);
			$zonecustom = base64_decode($zone['custom']);
			$zoneipslave = $zone['slaveip'];
			$zoneforwarders = $zone['forwarders'];
			if (!empty($zoneforwarders)) {
				$zoneforwarders = rtrim($zoneforwarders, ';');
			}
			// Ensure zone view folder exists
			if ($zonetype != "forward") {
				foreach ($zoneviewlist as $zoneview) {
					safe_mkdir(CHROOT_LOCALBASE . "/etc/namedb/{$zonetype}/{$zoneview}", 0755);
				}
			}

			if (empty($zone['allowupdate'])) {
				$zoneallowupdate = "none";
			} else {
				$zoneallowupdate = str_replace(',', '; ', $zone['allowupdate']);
			}
			$zoneupdatepolicy = $zone['updatepolicy'];
			if (!empty($zoneupdatepolicy)) {
				$zoneupdatepolicy = rtrim($zoneupdatepolicy, ';');
			}
			if (empty($zone['allowquery'])) {
				$zoneallowquery = "none";
			} else {
				$zoneallowquery = str_replace(',', '; ', $zone['allowquery']);
			}
			if (empty($zone['allowtransfer'])) {
				$zoneallowtransfer = "none";
			} else {
				$zoneallowtransfer = str_replace(',', '; ', $zone['allowtransfer']);
			}

			if (in_array($viewname, $zoneviewlist)) {
				$zoneview = $viewname;
				// Add zone name
				$bind_conf .= "\tzone \"" . reverse_zonename($zone['name'], $zone['reverso'], $zone['reversv6o']) . "\" {\n";
				$bind_conf .= "\t\ttype {$zone['type']};\n";

				// Add zone file (if not forwarder)
				if ($zone['type'] != "forward") {
					$bind_conf .= "\t\tfile \"/etc/namedb/{$zone['type']}/{$zoneview}/{$zone['name']}.DB\";\n";
				}

				// Add zone statements
				switch ($zonetype) {
					case 'master':
						$bind_conf .= "\t\tallow-query { $zoneallowquery; };\n";
						$bind_conf .= "\t\tallow-transfer { $zoneallowtransfer; };\n";
						if ($zone['enable_updatepolicy'] == "on") {
							$bind_conf .= "\t\tupdate-policy { $zoneupdatepolicy; };\n";
						} else {
							$bind_conf .= "\t\tallow-update { $zoneallowupdate; };\n";
						}
						if ($zone['dnssec'] == "on") {
							// https://kb.isc.org/article/AA-00626/
							$bind_conf .= "\n\t\t# look for dnssec keys here:\n";
							$bind_conf .= "\t\tkey-directory \"/etc/namedb/keys\";\n\n";
							$bind_conf .= "\t\t# publish and activate dnssec keys:\n";
							$bind_conf .= "\t\tauto-dnssec maintain;\n\n";
							$bind_conf .= "\t\t# use inline signing:\n";
							$bind_conf .= "\t\tinline-signing yes;\n\n";
						}
						break;

					case 'slave':
						$bind_conf .= "\t\tmasters { $zoneipslave; };\n";
						$bind_conf .= "\t\tallow-query { $zoneallowquery; };\n";
						if (!preg_match('/allow-transfer/', $zonecustom)) {
							$bind_conf .= "\t\tallow-transfer { $zoneallowtransfer; };\n";
						}
						break;

					case 'forward':
						$bind_conf .= "\t\tforward only;\n";
						$bind_conf .= "\t\tforwarders { $zoneforwarders; };\n";
						break;

					case 'redirect':
						$bind_conf .= "\t\t# While using redirect zones, NXDOMAIN Redirection will not override DNSSEC\n";
						$bind_conf .= "\t\t# If the client has requested DNSSEC records (DO=1) and the NXDOMAIN response is signed then no substitution will occur\n";
						$bind_conf .= "\t\t# https://kb.isc.org/article/AA-00376/192/BIND-9.9-redirect-zones-for-NXDOMAIN-redirection.html\n";
						$bind_conf .= "\t\tallow-query { $zoneallowquery; };\n";
						break;

					default:
						break;
				}

				// Add custom zone statements
				if ($zonecustom != '') {
					$bind_conf .= "\t\t{$zonecustom}\n";
				}

				$bind_conf .= "\t};\n\n";
			}
		}

		if (!$custom_root_zone && ($bind_forwarder != 'on')) {
			$bind_conf .= "\tzone \".\" {\n";
			$bind_conf .= "\t\ttype hint;\n";
			$bind_conf .= "\t\tfile \"/etc/namedb/named.root\";\n";
			$bind_conf .= "\t};\n\n";
		}

		$bind_conf .= "};\n";
	}
}

function sxbind_sync()
{
	global $config;
	$config_changed = false;
	$named_process = "named";
	$bind = $config['installedpackages'][ZoneParser::CONFIGPREFIX . 'bind']['config'][0];
	$bind_enable = $bind['enable_bind'];

	// Create rndc.conf file
	sxbind_create_rndc_conf_file(false, $rndc_key, $config_changed);

	//create named.conf string
	$bind_conf = sxbind_create_bind_conf($bind, $rndc_key, $config_changed);

	if (is_array($config['installedpackages'][ZoneParser::CONFIGPREFIX . 'bindzone'])) {
		$bindzone = $config['installedpackages'][ZoneParser::CONFIGPREFIX . 'bindzone']['config'];
	} else {
		$bindzone = array();
	}
	// Add ACLS to config
	if (!is_array($config['installedpackages']['sxbindacls']) || !is_array($config['installedpackages'][ZoneParser::CONFIGPREFIX . 'bindacls']['config'])) {
		$config['installedpackages'][ZoneParser::CONFIGPREFIX . 'bindacls']['config'][] =
			array("name" => "none", "description" => "BIND Built-in ACL", "row" => array("value" => "", "description" => ""));
		$config['installedpackages'][ZoneParser::CONFIGPREFIX . 'bindacls']['config'][] =
			array("name" => "any", "description" => "BIND Built-in ACL", "row" => array("value" => "", "description" => ""));
		$config['installedpackages'][ZoneParser::CONFIGPREFIX . 'bindacls']['config'][] =
			array("name" => "localhost", "description" => "BIND Built-in ACL", "row" => array("value" => "", "description" => ""));
		$config['installedpackages'][ZoneParser::CONFIGPREFIX . 'bindacls']['config'][] =
			array("name" => "localnets", "description" => "BIND Built-in ACL", "row" => array("value" => "", "description" => ""));
		write_config("Created BIND Built-in ACLs");
		$config_changed = true;
	}

	$bindacls = $config['installedpackages'][ZoneParser::CONFIGPREFIX . 'bindacls']['config'];
	sxbind_append_ACL_to_bind_conf($bind_conf, $bindacls);

	// Add Views
	if (is_array($config['installedpackages'][ZoneParser::CONFIGPREFIX . 'bindviews'])) {
		$bindview = $config['installedpackages'][ZoneParser::CONFIGPREFIX . 'bindviews']['config'];
	} else {
		$bindview = array();
	}
	sxbind_append_views_to_bind_conf($bind_conf, $bindview, $bindzone);

	for ($i = 0; $i < sizeof($bindview); $i++) {
		$views = $config['installedpackages'][ZoneParser::CONFIGPREFIX . 'bindviews']['config'][$i];
		$viewname = $views['name'];

		// Add Zones in View
		$write_config = 0;
		$custom_root_zone = false;
		for ($x = 0; $x < sizeof($bindzone); $x++) {
			$zone = $bindzone[$x];
			if ($zone['disabled'] == "on") {
				continue;
			}
			$zonename = $zone['name'];
			if ($zonename == ".") {
				$custom_root_zone = true;
			}
			$zonetype = $zone['type'];
			$zoneviewlist = explode(',', $zone['view']);
			$zoneforwarders = $zone['forwarders'];
			if (!empty($zoneforwarders)) {
				$zoneforwarders = rtrim($zoneforwarders, ';');
			}
			$zonereverso = $zone['reverso'];
			$zonereversv6o = $zone['reversv6o'];
			$zonerpz = (isset($zone["rpz"]) && ($zone["rpz"] === "on"));
			// Ensure zone view folder exists
			if ($zonetype != "forward") {
				foreach ($zoneviewlist as $zoneview) {
					safe_mkdir(CHROOT_LOCALBASE . "/etc/namedb/{$zonetype}/{$zoneview}", 0755);
				}
			}

			$zoneupdatepolicy = $zone['updatepolicy'];
			if (!empty($zoneupdatepolicy)) {
				$zoneupdatepolicy = rtrim($zoneupdatepolicy, ';');
			}

			if (in_array($viewname, $zoneviewlist)) {
				$zoneview = $viewname;

				// Create zone config DB file
				switch ($zonetype) {
					case 'master':
					case 'redirect':
						// Check/update slave dir permission
						chown(CHROOT_LOCALBASE . "/etc/namedb/{$zonetype}", "bind");
						chown(CHROOT_LOCALBASE . "/etc/namedb/{$zonetype}/{$zoneview}", "bind");

						$zonetll = $zone['tll'] ? $zone['tll'] : "43200";
						$zonemail = $zone['mail'] ? $zone['mail'] : "zonemaster.{$zonename}";
						$zonemail = $zonerpz ? "root.localhost" : preg_replace("/@/", ".", $zonemail);
						$zoneserial = $zone['serial'];
						$zonerefresh = $zone['refresh'] ? $zone['refresh'] : "3600";
						$zoneretry = $zone['retry'] ? $zone['retry'] : "600";
						$zoneexpire = $zone['expire'] ? $zone['expire'] : "86400";
						$zoneminimum = $zone['minimum'] ? $zone['minimum'] : "3600";
						$zonenameserver = rtrim($zonerpz ? "localhost" : $zone['nameserver'], '.');
						$zonenameserverip = $zonerpz ? "" : $zone['nameserverip'];
						$zoneipns = $zonerpz ? "" : $zone['ipns'];
						$zonereverso = $zone['reverso'];
						$zonereversv6o = $zone['reversv6o'];

						$zone_conf = "\$TTL {$zonetll}\n;\n";
						$zone_conf .= "\$ORIGIN " . reverse_zonename($zonename, $zonereverso, $zonereversv6o) . ".\n\n";
						$zone_conf .= ";\tDatabase file {$zonename}.DB for {$zonename} zone.\n";
						$zone_conf .= ";\tDo not edit this file!!!\n";
						$zone_conf .= ";\tZone version {$zoneserial}\n;\n";
						if (($zonereverso == "on") || ($zonetype == "redirect") || ($zonerpz)) {
							$zone_conf .= "@\t IN  SOA {$zonenameserver}. \t {$zonemail}. (\n";
						} else {
							$zone_conf .= "{$zonename}.\t IN  SOA {$zonenameserver}. \t {$zonemail}. (\n";
						}
						$zone_conf .= "\t\t{$zoneserial} ; serial\n"; // this line is used 1:1 to increment serial. if changing this line, change the reference too.
						$zone_conf .= "\t\t{$zonerefresh} ; refresh\n";
						$zone_conf .= "\t\t{$zoneretry} ; retry\n";
						$zone_conf .= "\t\t{$zoneexpire} ; expire\n";
						$zone_conf .= "\t\t{$zoneminimum} ; default_ttl\n\t\t)\n\n";
						$zone_conf .= ";\n; Zone Records\n;\n";

						if ($zonereverso == "on") {
							$zone_conf .= "\t IN NS \t{$zonenameserver}.\n";
							$zone_conf .= "{$zoneipns}. IN PTR {$zonenameserver}.\n";
						} else {
							$zone_conf .= "@ \t IN NS \t{$zonenameserver}.\n";
							if ($zoneipns != "") {
								$zone_conf .= "@ \t IN A \t{$zoneipns}\n";
							}
							if (is_ipaddrv6($zonenameserverip)) {
								$zone_conf .= "{$zonenameserver}. IN AAAA {$zonenameserverip}\n";
							} elseif (is_ipaddr($zonenameserverip)) {
								$zone_conf .= "{$zonenameserver}. IN A {$zonenameserverip}\n";
							}
						}

						// Register DHCP static mappings
						if (($zone['regdhcpstatic'] == 'on') && is_array($config['dhcpd'])) {
							$zoneparts = array_reverse(explode('.', $zonename));
							foreach ($config['dhcpd'] as $dhcpif => $dhcpifconf) {
								if (!isset($dhcpifconf['enable']) || !is_array($dhcpifconf['staticmap'])) {
									continue;
								}
								foreach ($dhcpifconf['staticmap'] as $host) {
									if (is_domain($host['domain'])) {
										$domain = $host['domain'];
									} elseif (is_domain($dhcpifconf['domain'])) {
										$domain = $dhcpifconf['domain'];
									} elseif (is_domain($config['system']['domain'])) {
										$domain = $config['system']['domain'];
									} else {
										continue;
									}
									if (!is_hostname($host['hostname']) || !is_ipaddr($host['ipaddr'])) {
										continue;
									}
									if ($zonereverso == "on") {
										$parts = explode('.', $host['ipaddr']);
										$intersect = array_intersect_assoc($parts, $zoneparts);
										if (count($zoneparts) == count($intersect)) {
											$diff = array_diff_assoc($parts, $zoneparts);
											$shortaddr = implode('.', array_reverse($diff));
											$zone_conf .= "{$shortaddr}\tIN PTR\t{$host['hostname']}.{$domain}.\n";
										}
									} else {
										$parts = array_reverse(explode('.', $domain));
										if ($parts === $zoneparts) {
											$zone_conf .= "{$host['hostname']}\tIN A\t{$host['ipaddr']}\n";
										}
									}
								}
							}
						}

						// Detect Zone changes
						$skipZoneUpdate = false;
						$diff = [];
						$zone_cache_file = CHROOT_LOCALBASE . "/etc/namedb/{$zonetype}/{$zoneview}/{$zonename}.confcache";
						$zonename_reverse = reverse_zonename($zonename, $zonereverso, $zonereversv6o);
						$rndc = "/usr/local/sbin/rndc -q -r -c " . BIND_LOCALBASE . "/etc/rndc.conf";
						$zonefile = CHROOT_LOCALBASE . "/etc/namedb/{$zonetype}/{$zoneview}/{$zonename}.DB";

						// read new and last zonedata to build diff
						if (file_exists($zone_cache_file)) {
							$zoneCacheString = file_get_contents($zone_cache_file);
							$zoneRows_cached_config = ZoneParser::parse_rndc_zone_dump($zoneCacheString, $zonename_reverse);
						} else {
							$zoneRows_cached_config = [];
						}

						// new records
						$zoneRows_new_config = ZoneParser::parse_rndc_zone_dump($zone_conf, $zonename_reverse);
						// removed records
						$diff = ZoneParser::diff_zonerecords($zoneRows_cached_config, $zoneRows_new_config);
						// all records which should not be in the dynamic section
						$diff = array_merge($zoneRows_new_config, $diff);
						if ($zoneCacheString == $zone_conf) {
							// no change in zone config. skip entire zone update.
							$skipZoneUpdate = true;
							log_error("[sxbind] INFO: Config file for zone {$zonename} skipped cause nothing changed.");
						}

						if (!$skipZoneUpdate) {
							$process_running = is_process_running($named_process);
							$zone_conf_orig = $zone_conf;
							$zone_conf_dynamic = '';
							$current_zone_data_parsed = null;

							// Freeze dynamic zones to prevent journal corruption.
							if ($process_running && file_exists($zonefile)) {
								ZoneParser::freeze_zone($zoneview, $zonename_reverse);
							}

							if (file_exists($zonefile)) {
								copy($zonefile, $zonefile . '.backup');
							}

							if ($zone['reset_zone'] == "on") {
								unlink($zonefile);
								unset($zone['reset_zone']);
								$write_config++;
							}

							// read current zone data
							if (file_exists($zonefile)) {
								$current_zone_data_parsed = ZoneParser::get_zone_dump_parsed($zonetype, $zoneview, $zonename_reverse);
								if (!$current_zone_data_parsed) {
									$error = "[sxbind] ERROR: Failed to parse current zone data of zone {$zonename_reverse} .To prevent loose , the Zone remains in the old state. Manual resolve any errors in zone file or performe a reset on the zone from the zone config to fix this issue.\n\n";
									file_notice("named_config", $error, "sxbind DNS", "", 2);
									log_error($error);

									// Skip Update
									continue;
								}
							}

							// compare and merge zone data by adding existing records in db to zone_conf_dynamic
							if ($current_zone_data_parsed) {
								$zonedata_to_merge = ZoneParser::diff_zonerecords($current_zone_data_parsed, $diff);
								$zone_conf_dynamic = "\n;\n; Merged Dynamic Zone Records\n;\n";
								foreach ($zonedata_to_merge as $row) {
									if ($row['type'] !== 'SOA') {
										// Add all records except SOA as this is already added to zone_conf.
										$zone_conf_dynamic .= ZoneParser::zone_record_to_string($row) . "\n";
									}
								}
								log_error('[sxbind] INFO: ZONE ' . $zonename_reverse . ' : ' . count($current_zone_data_parsed) . ' Records in DB. ' . count($zonedata_to_merge) . ' Records to keep from DB. ' . count($zoneRows_new_config) . ' Records to add from Config.');
							}

							//Increment serial 
							if ($zone['increment_serial'] == "on") {
								$current_zoneserial = null;
								// get current serial from zone db
								if ($current_zone_data_parsed) {
									foreach ($current_zone_data_parsed as $row) {
										if ($row["type"] == 'SOA') {
											$current_zoneserial = $row['serial'];
											break;
										}
									}
								}
								if ($current_zoneserial) {
									$zone_conf = sxbind_set_serial_zoneconf($zone_conf, $current_zoneserial + 1);
								}
							}

							// validate and save to DB if successfull.
							$result = ZoneParser::save_zonefile($zone_conf . $zone_conf_dynamic, $zonetype, $zoneview, $zonename_reverse);

							if ($result['success']) {
								/* Validation successfull
																																																																																																																																																																																							Save zone_conf to cache file for comparison on next update. */
								file_put_contents($zone_cache_file, $zone_conf_orig);
								$data = base64_encode($zone_conf . $zone_conf_dynamic);
								if ($config['installedpackages'][ZoneParser::CONFIGPREFIX . 'bindzone']['config'][$x]['resultconfig'] != $data) {
									$config['installedpackages'][ZoneParser::CONFIGPREFIX . 'bindzone']['config'][$x]['resultconfig'] = $data;
									$write_config++;
								}

							} else {
								// Validation failed. Keep old zone DB and send error notice.
								$error = "[sxbind] VALIDATION FAILED: Zone {$zonename_reverse} not saved. Code {$result['result_code']}\n" . $result['message'];
								file_notice("named_config", $error, "sxbind DNS", "", 2);
								log_error($error);
							}

							if ($process_running) {
								// Thaw frozen zone
								$result = ZoneParser::thaw_zone($zoneview, $zonename_reverse);
								if (!$result['success']) {
									$error = "[sxbind] WARNING: RNDC THAW throwed an exception. Zone {$zonename_reverse} may still be frozen. Code {$result['result_code']}\n" . $result['message'];
									log_error($error);
									file_notice("named_config", $error, "sxbind DNS", "", 1);
								}
							}
						}


						// Check DNSSEC keys creation for master zones
						if ($zone['dnssec'] == "on") {
							$zone_found = 0;
							/* move DNSSEC keys from old chroot,
							 * see https://redmine.pfsense.org/issues/13002 */
							foreach (glob("/var/etc/named/etc/namedb/keys/K{$zonename}*key", GLOB_NOSORT) as $filename) {
								$newkey = CHROOT_LOCALBASE . "/etc/namedb/keys/" . basename($filename);
								if (!file_exists($newkey)) {
									rename($filename, $newkey);
								}
							}
							foreach (glob(CHROOT_LOCALBASE . "/etc/namedb/keys/K{$zonename}*key", GLOB_NOSORT) as $filename) {
								$zone_found++;
							}
							if ($zone_found == 0) {
								$key_restored = 0;
								if (is_array($config['installedpackages']['dnsseckeys']) && is_array($config['installedpackages']['dnsseckeys']['config'])) {
									foreach ($config['installedpackages']['dnsseckeys']['config'] as $filer) {
										if (preg_match("/K$zonename\.+/", $filer['fullfile'])) {
											file_put_contents($filer['fullfile'], base64_decode($filer['filedata']), LOCK_EX);
											chmod($filer['fullfile'], 0700);
											chown($filer['fullfile'], "bind");
											$key_restored++;
										}
									}
								}
								if ($key_restored > 0) {
									log_error("[sxbind] DNSSEC RESTORED: {$key_restored} DNSSEC keys restored from XML backup for {$zonename} zone.");
								}
								$dnssec_bin = "/usr/local/sbin/dnssec-keygen";
								$z = reverse_zonename($zonename, $zonereverso, $zonereversv6o);
								if (file_exists($dnssec_bin) && $key_restored == 0) {
									exec("{$dnssec_bin} -a ECDSAP256SHA256 -K " . CHROOT_LOCALBASE . "/etc/namedb/keys {$z}", $kout);
									exec("{$dnssec_bin} -a ECDSAP256SHA256 -K " . CHROOT_LOCALBASE . "/etc/namedb/keys -fk {$z}", $kout);
									foreach ($kout as $filename) {
										chown(CHROOT_LOCALBASE . "/etc/namedb/keys/{$filename}.key", "bind");
										chown(CHROOT_LOCALBASE . "/etc/namedb/keys/{$filename}.private", "bind");
									}
									log_error("[sxbind] DNSSEC CREATED: DNSSEC keys for {$zonename} created.");
								}
							}
							// Get DS keys
							$dsfromkey = "/usr/local/sbin/dnssec-dsfromkey";
							foreach (glob(CHROOT_LOCALBASE . "/etc/namedb/keys/K{$zonename}*key", GLOB_NOSORT) as $filename) {
								$zone_key = file_get_contents($filename);
								if (preg_match("/IN DNSKEY 257 /", $zone_key) && file_exists($dsfromkey)) {
									exec("$dsfromkey $filename", $dsset);

									$data = base64_encode(array_pop($dsset) . "\n" . array_pop($dsset));
									if ($config['installedpackages'][ZoneParser::CONFIGPREFIX . 'bindzone']['config'][$x]['dsset'] != $data) {
										$config['installedpackages'][ZoneParser::CONFIGPREFIX . 'bindzone']['config'][$x]['dsset'] = $data;
										$write_config++;
									}
								}
							}

							// Save DNSSEC keys to xml
							if ($zone['backupkeys'] == "on") {
								$dnssec_keys = 0;
								foreach (glob(CHROOT_LOCALBASE . "/etc/namedb/keys/K{$zonename}*", GLOB_NOSORT) as $filename) {
									$file_found = 0;
									if (is_array($config['installedpackages']['dnsseckeys']) && is_array($config['installedpackages']['dnsseckeys']['config'])) {
										foreach ($config['installedpackages']['dnsseckeys']['config'] as $filer) {
											if ($filer['fullfile'] == $filename) {
												$file_found++;
											}
										}
									}
									if ($file_found == 0) {
										$data = array(
											'fullfile' => $filename,
											'description' => "sxbind {$zonename} DNSSEC backup file",
											'filedata' => base64_encode(file_get_contents($filename))
										);
										$config['installedpackages']['dnsseckeys']['config'][] = $data;
										$write_config++;
										$dnssec_keys++;
									}
								}
								if ($dnssec_keys > 0) {
									log_error("[sxbind] DNSSEC SAVED: {$dnssec_keys} DNSSEC keys for {$zonename} zone saved on XML config.");
								}
							}
						}
						break;
					case 'slave':
						// Check/update slave dir permission
						chown(CHROOT_LOCALBASE . "/etc/namedb/{$zonetype}", "bind");
						chown(CHROOT_LOCALBASE . "/etc/namedb/{$zonetype}/{$zoneview}", "bind");
						// check if exists slave zone file
						$rsconfig = "";
						if ($zone['dnssec'] == "on") {
							if (file_exists(CHROOT_LOCALBASE . "/etc/namedb/{$zonetype}/{$zoneview}/{$zonename}.DB.signed")) {
								exec("/usr/local/sbin/named-checkzone -D -f raw -o - {$zonename} " . CHROOT_LOCALBASE . "/etc/namedb/{$zonetype}/{$zoneview}/{$zonename}.DB.signed", $slave_file);
							}
						} else {
							if (file_exists(CHROOT_LOCALBASE . "/etc/namedb/{$zonetype}/{$zoneview}/{$zonename}.DB")) {
								$slave_file = file(CHROOT_LOCALBASE . "/etc/namedb/{$zonetype}/{$zoneview}/{$zonename}.DB");
							}
						}
						if ($slave_file) {
							foreach ($slave_file as $zfile) {
								$rsconfig .= $zfile;
							}
						}

						$data = base64_encode($rsconfig);
						if ($config['installedpackages'][ZoneParser::CONFIGPREFIX . 'bindzone']['config'][$x]['resultconfig'] != $data) {
							$config['installedpackages'][ZoneParser::CONFIGPREFIX . 'bindzone']['config'][$x]['resultconfig'] = $data;
							$write_config++;
						}

						break;

					case 'forward':
						// Forwarder zone does not have a DB file
						if ($config['installedpackages'][ZoneParser::CONFIGPREFIX . 'bindzone']['config'][$x]['resultconfig'] != '') {
							$config['installedpackages'][ZoneParser::CONFIGPREFIX . 'bindzone']['config'][$x]['resultconfig'] = '';
							$write_config++;
						}
						break;
					default:
						break;
				}
			}
		}
		if ($write_config > 0) {
			write_config("sxbind: Saved resulting config file for zone in xml");
		}
	}
	$dirs = array("/etc/namedb/keys", "/var/run/named", "/var/dump", "/var/log", "/var/stats", "/dev");
	foreach ($dirs as $dir) {
		safe_mkdir(CHROOT_LOCALBASE . $dir, 0755);
	}
	// Handle /dev dirs for chroot
	$bind_dev_dir = CHROOT_LOCALBASE . "/dev";
	if (!file_exists("$bind_dev_dir/random")) {
		$dev_dirs = array("null", "zero", "random", "urandom");
		exec("/sbin/mount -t devfs devfs {$bind_dev_dir}", $dout);
		exec("/sbin/devfs -m {$bind_dev_dir} ruleset 1", $dout);
		exec("/sbin/devfs -m {$bind_dev_dir} rule add hide", $dout);
		foreach ($dev_dirs as $dev_dir) {
			exec("/sbin/devfs -m {$bind_dev_dir} rule add path {$dev_dir} unhide", $dout);
		}
		exec("/sbin/devfs -m {$bind_dev_dir} rule applyset", $dout);
	}

	// Handle /usr/local/lib/named for chroot
	$bind_plugin_dir = CHROOT_LOCALBASE . BIND_LOCALBASE . "/lib/named";
	rmdir_recursive($bind_plugin_dir);
	safe_mkdir($bind_plugin_dir, 0755);

	foreach (glob(BIND_LOCALBASE . "/lib/named/*.so") as $bind_plugin) {
		copy($bind_plugin, CHROOT_LOCALBASE . $bind_plugin);
	}
	// http://www.unixwiz.net/techtips/bind9-chroot.html
	file_put_contents_if_changed(CHROOT_LOCALBASE . '/etc/namedb/named.conf', $bind_conf, $config_changed);

	if (!file_exists(CHROOT_LOCALBASE . "/etc/namedb/named.root")) {
		$curl_session = curl_init();
		curl_setopt($curl_session, CURLOPT_URL, "http://www.internic.net/domain/named.root");
		curl_setopt($curl_session, CURLOPT_RETURNTRANSFER, true);
		curl_setopt($curl_session, CURLOPT_USERAGENT, $g['product_label'] . '/' . rtrim(file_get_contents("/etc/version")));
		set_curlproxy($curl_session);
		$named_root = curl_exec($curl_session);
		if (curl_errno($curl_session) || empty($named_root)) {
			$named_root = file_get_contents("/usr/local/etc/namedb/named.root");
		}
		curl_close($curl_session);
		file_put_contents(CHROOT_LOCALBASE . "/etc/namedb/named.root", $named_root, LOCK_EX);
		$config_changed = true;
	}
	if (!file_exists(CHROOT_LOCALBASE . "/etc/localtime")) {
		copy("/etc/localtime", CHROOT_LOCALBASE . "/etc/localtime");
		$config_changed = true;
	}

	sxbind_write_rcfile();

	chown(CHROOT_LOCALBASE . "/etc/namedb/keys", "bind");
	chown(CHROOT_LOCALBASE . "/etc/namedb", "bind");
	chown(CHROOT_LOCALBASE . "/var/log", "bind");
	chown(CHROOT_LOCALBASE . "/var/run/named", "bind");
	chgrp(CHROOT_LOCALBASE . "/var/log", "bind");
	$bind_sh = "/usr/local/etc/rc.d/named.sh";

	if ($bind_enable == "on") {
		chmod($bind_sh, 0755);
		if (!$restart_required && is_process_running($named_process)) {
			if ($config_changed) {
				exec("{$rndc} reload" . ' 2>&1', $output, $retval);
				if ($retval !== 0) {
					// restart service as Fallback to rndc reload
					log_error("[sxbind] WARNING: \"RNDC reload\" CODE {$retval}:\n" . implode("\n", $output));
					restart_service("named");
					log_error("[sxbind] RESTART SERVICE: service restarted because reconfig throwed an exception.");
				}
			} else {
				log_error("[sxbind] INFO: Reload of BIND skipped because no changes detected.");
			}

		} else {
			restart_service("named");
		}
	} else {
		stop_service("named");
		chmod($bind_sh, 0644);
	}
	// Sync to backup servers
	sxbind_sync_on_changes();
}

function sxbind_print_javascript_type_zone()
{
	$js = <<<'JS'
	<script>
	events.push(function(){
		var $on_zonename_changed_timer = null

		$("input#reversv6o").closest(".form-group").hide()
		$("input#tempconverttoreverse").closest(".form-group").hide()
		$("textarea#resultconfig").attr("disabled", true)
		$("textarea#dsset").attr("disabled", true)
		$("input#tempfullzonename").attr("readonly","readonly")

		$("select#type").on('input', on_type_zone_changed)

		$("input#reverso").on('input',function(){
			if (!$("#reverso").is(':checked')){
				$("#reversv6o").prop("checked","")
				$("input#tempconverttoreverse").closest(".form-group").hide()
				$("input#reversv6o").closest(".form-group").hide()
			}else{
				$("input#tempconverttoreverse").closest(".form-group").show()
				$("input#reversv6o").closest(".form-group").show()
			}
			on_zonename_changed()
		})

		$("input#reversv6o").on('input', function(){
			if ($("#reversv6o").is(':checked')){$("#reverso").prop("checked", true)}
			on_zonename_changed()
		})

		$("input#tempconverttoreverse").on('input', on_zonename_changed)

		function on_zonename_changed() {
			$("input#name").val("[calculating name....]")
			$("input#tempfullzonename").val("[calculating name....]")
			
			clearTimeout($on_zonename_changed_timer)
			$on_zonename_changed_timer = setTimeout(getPTR, 1000);
		}

		function getPTR(){
			$.ajax({
                type: 'post',
				url: '/sxbind_tools.php',
                data: {
                    action: "convert_ip_to_ptr",
					ip: $("#tempconverttoreverse").val()
                },
                success: function (data) {
					data = data.toUpperCase();
					if (data.endsWith('.IP6.ARPA')){
						$("#reversv6o").prop("checked","true")
						$("#reversv6o").val(true);
                    	$("input#tempfullzonename").val(data)
						$("input#name").val(data.replace('.IP6.ARPA',''))

					}else if (data.endsWith('.IN-ADDR.ARPA')){
						$("#reversv6o").prop("checked","")
						$("#reversv6o").val(false);
                    	$("input#tempfullzonename").val(data)
						$("input#name").val(data.replace('.IN-ADDR.ARPA',''))

					}else{
                    	$("input#tempfullzonename").val("")
					}
                },
                error: function (data) {
                  	$("input#name").val("[error]")
					$("input#tempfullzonename").val("[error]")
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
	});
	</script>
JS;
	echo $js;
}

function sxbind_write_rcfile()
{
	global $config;
	$bind = $config['installedpackages'][ZoneParser::CONFIGPREFIX . 'bind']['config'][0];
	$ip_version = ($bind['bind_ip_version'] ? $bind['bind_ip_version'] : "");
	$rc = array();
	$BIND_LOCALBASE = "/usr/local";
	$rc['file'] = 'named.sh';
	// Curly braces in the following <<<EOD are PHP {$variable}, not named.conf text { value; }
	$rc['start'] = <<<EOD
	if [ -z "`/bin/ps auxw | /usr/bin/grep "[n]amed " | /usr/bin/awk '{print $2}'`" ]; then
		{$BIND_LOCALBASE}/sbin/named {$ip_version} -c /etc/namedb/named.conf -u bind -t /var/etc/named/
	fi
EOD;
	$rc['stop'] = <<<EOD
	if [ -n "`/bin/ps auxw | /usr/bin/grep "[n]amed " | /usr/bin/awk '{print $2}'`" ]; then
		{$BIND_LOCALBASE}/sbin/rndc -q -r -c "{$BIND_LOCALBASE}/etc/rndc.conf" sync -clean 2>/dev/null
		{$BIND_LOCALBASE}/sbin/rndc -q -r -c "{$BIND_LOCALBASE}/etc/rndc.conf" stop -clean 2>/dev/null
		sleep 1
		/usr/bin/killall -TERM named 2>/dev/null
		sleep 2
	fi
EOD;
	// curly braces in the following <<<EOD are PHP {$variable}, not named.conf text { value; }
	$rc['restart'] = <<<EOD
	if [ -z "`/bin/ps auxw | /usr/bin/grep "[n]amed " | /usr/bin/awk '{print $2}'`" ]; then
		{$BIND_LOCALBASE}/sbin/named {$ip_version} -c /etc/namedb/named.conf -u bind -t /var/etc/named/
	else
		{$BIND_LOCALBASE}/sbin/rndc -q -r -c "{$BIND_LOCALBASE}/etc/rndc.conf" sync -clean 2>/dev/null
		{$BIND_LOCALBASE}/sbin/rndc -q -r -c "{$BIND_LOCALBASE}/etc/rndc.conf" stop -clean 2>/dev/null
		sleep 1
		/usr/bin/killall -TERM named 2>/dev/null
		sleep 3
		{$BIND_LOCALBASE}/sbin/named {$ip_version} -c /etc/namedb/named.conf -u bind -t /var/etc/named/
	fi
EOD;
	write_rcfile($rc);
}

/* Uses XMLRPC to synchronize the changes to a remote node */
function sxbind_sync_on_changes()
{
	global $config, $g;

	if (is_array($config['installedpackages'][ZoneParser::CONFIGPREFIX . 'bindsync']['config'])) {
		$bind_sync = $config['installedpackages'][ZoneParser::CONFIGPREFIX . 'bindsync']['config'][0];
		$synconchanges = $bind_sync['synconchanges'];
		$synctimeout = (int) $bind_sync['synctimeout'] ?: 30;
		$master_zone_ip = $bind_sync['masterip'];
		switch ($synconchanges) {
			case 'manual':
				if (is_array($bind_sync['row'])) {
					$rs = $bind_sync['row'];
				} else {
					log_error("[sxbind] WARNING: XMLRPC sync is enabled but there are no hosts to push on BIND config.");
					return;
				}
				break;
			case 'auto':
				if (is_array($config['hasync'])) {
					$hasync = $config['hasync'];
					$rs[0]['ipaddress'] = $hasync['synchronizetoip'];
					$rs[0]['username'] = $hasync['username'];
					$rs[0]['password'] = $hasync['password'];
					$rs[0]['syncdestinenable'] = FALSE;

					// XMLRPC sync is currently only supported over connections using the same protocol and port as this system
					if ($config['system']['webgui']['protocol'] == "http") {
						$rs[0]['syncprotocol'] = "http";
						$rs[0]['syncport'] = $config['system']['webgui']['port'] ?: '80';
					} else {
						$rs[0]['syncprotocol'] = "https";
						$rs[0]['syncport'] = $config['system']['webgui']['port'] ?: '443';
					}
					if ($hasync['synchronizetoip'] == "") {
						log_error("[sxbind] WARNING: XMLRPC CARP/HA sync is enabled but there are no system backup hosts configured as replication targets.");
						return;
					} else {
						$rs[0]['syncdestinenable'] = TRUE;
					}
				} else {
					log_error("[sxbind] WARNING: XMLRPC sync is enabled but there are no system backup hosts to push BIND config.");
					return;
				}
				break;
			default:
				return;
		}
		if (is_array($rs)) {
			log_error("[sxbind] INFO: XMLRPC sync is starting.");
			foreach ($rs as $sh) {
				// Only sync enabled replication targets
				if ($sh['syncdestinenable']) {
					$sync_to_ip = $sh['ipaddress'];
					$port = $sh['syncport'];
					/* If port is empty let's rely on the protocol selection */
					if ($port == "") {
						if ($config['system']['webgui']['protocol'] == "http") {
							$port = "80";
						} else {
							$port = "443";
						}
					}
					$username = $sh['username'] ?: 'admin';
					$password = $sh['password'];
					$protocol = $sh['syncprotocol'];
					/* If protocol is empty let's rely on the webGUI protocol selection */
					if ($protocol == "") {
						if ($config['system']['webgui']['protocol'] == "http") {
							$protocol = "http";
						} else {
							$protocol = "https";
						}
					}

					$error = '';
					$valid = TRUE;

					if ($password == "") {
						$error = "Password parameter is empty. ";
						$valid = FALSE;
					}
					if (!is_ipaddr($sync_to_ip) && !is_hostname($sync_to_ip) && !is_domain($sync_to_ip)) {
						$error .= "Misconfigured Replication Target IP Address or Hostname. ";
						$valid = FALSE;
					}
					if (!is_port($port)) {
						$error .= "Misconfigured Replication Target Port. ";
						$valid = FALSE;
					}
					if ($master_zone_ip == "") {
						$error .= "Zone master IP parameter is empty. ";
						$valid = FALSE;
					} elseif (!is_ipaddr($master_zone_ip)) {
						$error .= "Misconfigured zone master IP Address. ";
						$valid = FALSE;
					}
					if ($valid) {
						sxbind_do_xmlrpc_sync($sync_to_ip, $port, $protocol, $username, $password, $synctimeout, $master_zone_ip);
					} else {
						log_error("[sxbind] ERROR: XMLRPC sync with '{$sync_to_ip}' aborted due to the following error(s): {$error}");
					}
				}
			}
			log_error("[sxbind] INFO: XMLRPC sync completed.");
		}
	}
}

/* Do the actual XMLRPC sync */
function sxbind_do_xmlrpc_sync($sync_to_ip, $port, $protocol, $username, $password, $synctimeout, $master_zone_ip)
{
	global $config, $g;

	if (($username == "") || ($password == "") || ($sync_to_ip == "") || ($port == "") || ($protocol == "") || ($master_zone_ip == "")) {
		log_error("[sxbind] ERROR: A required XMLRPC sync parameter (username, password, replication target, port, protocol or zone master IP) is empty ... aborting pkg sync");
		return;
	}

	/* XML will hold the sections to sync */
	$xml = array();
	$xml['sxbind'] = $config['installedpackages'][ZoneParser::CONFIGPREFIX . 'bind'];
	$xml['sxbindacls'] = $config['installedpackages'][ZoneParser::CONFIGPREFIX . 'bindacls'];
	$xml['sxbindviews'] = $config['installedpackages'][ZoneParser::CONFIGPREFIX . 'bindviews'];
	$xml['sxbindzone'] = $config['installedpackages'][ZoneParser::CONFIGPREFIX . 'bindzone'];
	if (is_array($config['installedpackages']['dnsseckeys'])) {
		$xml['dnsseckeys'] = $config['installedpackages']['dnsseckeys'];
	}
	// Change master zone to slave on backup servers
	if (is_array($xml['sxbindzone']["config"])) {
		for ($x = 0; $x < sizeof($xml[ZoneParser::CONFIGPREFIX . 'bindzone']["config"]); $x++) {
			if ($xml['sxbindzone']["config"][$x]['type'] == "master") {
				$xml['sxbindzone']["config"][$x]['type'] = "slave";
				$xml['sxbindzone']["config"][$x]['slaveip'] = $master_zone_ip;
			}
		}
	}

	/* Commands to reload BIND settings on the destination sync host. */
	$execcmd = "require_once('/usr/local/pkg/sxbind.inc');\n";
	$execcmd .= "sxbind_sync('yes');";

	if (pf_version() >= "2.4") {
		// xmlrpc cannot encode NULL objects/arrays..
		foreach ($xml as $xmlkey => $xmlvalue) {
			if (gettype($xmlvalue) == "NULL") {
				$xml[$xmlkey] = array();
			}
		}
		$rpc_client = new pfsense_xmlrpc_client();
		$rpc_client->setConnectionData($sync_to_ip, $port, $username, $password, $protocol);
		$resp = $rpc_client->xmlrpc_method('merge_installedpackages_section', $xml, $synctimeout);
		$resp = $rpc_client->xmlrpc_exec_php($execcmd, $synctimeout);
	} else {
		// pfSense before 2.4
		require_once('xmlrpc.inc');

		// Take care of IPv6 literal address
		if (is_ipaddrv6($sync_to_ip)) {
			$sync_to_ip = "[{$sync_to_ip}]";
		}

		$url = "{$protocol}://{$sync_to_ip}";

		/* Assemble XMLRPC payload */
		$params = array(
			XML_RPC_encode($password),
			XML_RPC_encode($xml)
		);

		/* Set a few variables needed for sync */
		log_error("[sxbind] INFO: Beginning bind XMLRPC sync to {$url}:{$port}.");
		$method = 'pfsense.merge_installedpackages_section_xmlrpc';
		$msg = new XML_RPC_Message($method, $params);
		$cli = new XML_RPC_Client('/xmlrpc.php', $url, $port);
		$cli->setCredentials($username, $password);
		if ($g['debug']) {
			$cli->setDebug(1);
		}
		/* Send our XMLRPC message and timeout after defined sync timeout value */
		$resp = $cli->send($msg, $synctimeout);
		if (!$resp) {
			$error = "A communication error occurred while attempting BIND XMLRPC sync with {$url}:{$port}.";
			log_error($error);
			file_notice("sync_settings", $error, "sxbind Settings Sync", "");
		} elseif ($resp->faultCode()) {
			$cli->setDebug(1);
			$resp = $cli->send($msg, $synctimeout);
			$error = "An error code was received while attempting BIND XMLRPC sync with {$url}:{$port} - Code " . $resp->faultCode() . ": " . $resp->faultString();
			log_error($error);
			file_notice("sync_settings", $error, "sxbind Settings Sync", "");
		} else {
			log_error("[sxbind] INFO: XMLRPC sync successfully completed with {$url}:{$port}.");
		}

		/* Tell bind to reload our settings on the destination sync host. */
		$method = 'pfsense.exec_php';
		/* Assemble XMLRPC payload */
		$params = array(
			XML_RPC_encode($password),
			XML_RPC_encode($execcmd)
		);

		log_error("[sxbind] INFO: XMLRPC reload data {$url}:{$port}.");
		$msg = new XML_RPC_Message($method, $params);
		$cli = new XML_RPC_Client('/xmlrpc.php', $url, $port);
		$cli->setCredentials($username, $password);
		$resp = $cli->send($msg, $synctimeout);
		if (!$resp) {
			$error = "A communication error occurred while attempting sxbind XMLRPC sync with {$url}:{$port} (pfsense.exec_php).";
			log_error($error);
			file_notice("sync_settings", $error, "sxbind Settings Sync", "");
		} elseif ($resp->faultCode()) {
			$cli->setDebug(1);
			$resp = $cli->send($msg, $synctimeout);
			$error = "[sxbind] ERROR: An error code was received while attempting sxbind XMLRPC sync with {$url}:{$port} - Code " . $resp->faultCode() . ": " . $resp->faultString();
			log_error($error);
			file_notice("sync_settings", $error, "sxbind Settings Sync", "");
		} else {
			log_error("sxbind XMLRPC reload data success with {$url}:{$port} (pfsense.exec_php).");
		}
	}
}
function reverse_zonename($zonename, $zonereverso = false, $zonereversv6o = false)
{
	if ($zonereverso == "on") {
		if ($zonereversv6o == "on") {
			return $zonename . ".ip6.arpa";
		} else {
			return $zonename . ".in-addr.arpa";
		}
	} else {
		return $zonename;
	}
}
?>
