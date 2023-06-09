<?php
/*
 * sxbind_zoneparser.inc
 *
 * Copyright (c) 2023 Andreas W. Pross (Styletronix.net)
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

/**
 * Summary of namespace binddump
 */

namespace sxbind {
    require_once("globals.inc");
    require_once("config.inc");
    require_once("util.inc");
    require_once("pfsense-utils.inc");
    require_once("pkg-utils.inc");
    require_once("/usr/local/pkg/sxbind.inc");

    class ZoneParser
    {
        const BIND_LOCALBASE = '/usr/local';
        const CHROOT_LOCALBASE = '/var/etc/named';
        const CONFIGPREFIX = 'sx';
        const RECORDTYPES_EXPANDABLE = ['A', 'AAAA', 'CNAME', 'TXT', 'SPF', 'PTR', 'MX', 'SOA', 'SRV', 'NS'];
        const RECORDTYPES = [
            'A',
            'AAAA',
            'CNAME',
            'TXT',
            'SPF',
            'PTR',
            'MX',
            'SOA',
            'SRV',
            'NS',
            'DCHID',
            'CERT',
            'DNSKEY',
            'RRSIG',
            'CDNSKEY',
            'NSEC',
            'TA',
            'IPSECKEY',
            'KEY',
            'DNAME',
            'AFSDB',
            'APL',
            'CAA',
            'CDS',
            'CSYNC',
            'DLV',
            'DS',
            'EUI48',
            'EUI64',
            'HINFO',
            'HIP',
            'HTTPS',
            'KX',
            'NAPTR',
            'LOC',
            'OPENPGPKEY',
            'NSEC3',
            'NSEC3PARAM',
            'RP',
            'SIG',
            'SMIMEA',
            'SSHFP',
            'SVCB',
            'TKEY',
            'TSIG',
            'TLSA',
            'ZONEMD',
            'URI',
            'AXFR',
            'TLSA'
        ];
        public static $PATH_CHECKZONE;
        public static $PATH_COMPILEZONE;
        const PATH_RNDC_CONF = self::BIND_LOCALBASE . '/etc/rndc.conf';
        const CMD_RNDC = '/usr/local/sbin/rndc -r -c "' . self::PATH_RNDC_CONF . '" ';
        const PATH_DUMPFILE = self::CHROOT_LOCALBASE . '/etc/namedb/named_dump.db';
        /**
         * Summary of freeze_zone
         * @param mixed $zoneview  Zoneview
         * @param mixed $fullzonename The full zone name including ...arpa for reverse zones if applicable.
         * @return array result_code = resultode from rndc freeze. message = resultmessage. success = true if successfull.
         */
        public static function freeze_zone(string $zoneview, string $fullzonename)
        {
            $rndc = self::CMD_RNDC;

            if (str_ends_with($zoneview, ' (signed)')) {
                $zoneview = substr($zoneview, 0, -9);
            }

            exec("{$rndc} freeze " . escapeshellarg($fullzonename) . " IN " . escapeshellarg($zoneview) . ' 2>&1', $output, $resultCode);

            return [
                'result_code' => $resultCode,
                'message' => implode("\n", $output),
                'success' => ($resultCode == 0 ? true : false)
            ];
        }


        public static function thaw_zone(string $view, string $zonename)
        {
            $rndc = self::CMD_RNDC;
            $zonename = rtrim($zonename, '.');
            if (str_ends_with($view, ' (signed)')) {
                $view = substr($view, 0, -9);
            }

            exec("{$rndc} thaw " . escapeshellarg($zonename) . " IN " . escapeshellarg($view) . ' 2>&1', $output, $resultCode);

            return [
                'result_code' => $resultCode,
                'message' => implode("\n", $output),
                'success' => ($resultCode == 0 ? true : false)
            ];
        }
        public static function thaw_all()
        {
            $rndc = self::CMD_RNDC;
            exec("{$rndc} thaw " . ' 2>&1', $output, $resultCode);

            return [
                'result_code' => $resultCode,
                'message' => implode("\n", $output),
                'success' => ($resultCode == 0 ? true : false)
            ];
        }

        /**
         * Validates and writes data to the zonefile. It is required to freeze the zone with freeze_zone() prior to calling this function.
         * Call thaw_zone() after save.
         * @param string $data  Data to be stored in the zone file as string.
         * @param string $zonetype  Zonetype [master|slave]
         * @param string $view   Zoneview
         * @param string $zonename   The full zone name including ...arpa for reverse zones if applicable.
         * @return array result_code = resultode from checkzone. message = resultmessage. success = true if successfull.
         */
        public static function save_zonefile(string $data, string $zonetype, string $view, string $zonename)
        {
            global $config;
            // Replace all kind of linebreak styles with LF (unix style)
            $data = preg_replace('~\R~u', "\n", $data) . "\n";
            $dataexploded = explode("\n", $data);

            // parse zone data and increment serial
            $zone_compiled = self::parse_rndc_zone_dump($data, $zonename);
            foreach ($zone_compiled as $record) {
                if ($record['type'] == 'SOA') {
                    $soa_record = $record;
                    break;
                }
            }

            if ($soa_record) {
                $soa_record['serial']++;

                // combine raw text format of data with parsed soa record
                $data1 = array_slice($dataexploded, 0, $soa_record['_index']);
                self::collapse_zone_record($soa_record);
                $data2 = array_slice($dataexploded, $soa_record['_index'] + $soa_record['_rowcount']);

                $dataMerged = $data1;
                $dataMerged[] = self::zone_record_to_string($soa_record);
                $dataMerged = array_merge($dataMerged, $data2);

                $data = implode("\n", $dataMerged);
                
                log_error('[bind] Serial has been incremented to ' . $soa_record['serial']);
            } else {
                log_error('[bind] Serial in zone data not found!');
            }

            $zonename = rtrim($zonename, '.');
            $rzone = self::re_reverse_zonename($zonename);
            $tempDB = tempnam("/tmp", "validate_zone");
            file_put_contents($tempDB, $data);

            if (str_ends_with($view, ' (signed)')) {
                $view = substr($view, 0, -9);
            }

            // validate and save to DB if successfull.
            // Defaut format text for master zones.
            // TODO: detect the zone format. text|raw|raw=|map
            exec(self::$PATH_CHECKZONE . ' -F text ' .
                '-o ' . escapeshellarg(CHROOT_LOCALBASE . "/etc/namedb/{$zonetype}/{$view}/{$rzone}.DB") . ' ' .
                escapeshellarg($zonename) . ' ' .
                escapeshellarg($tempDB) . ' 2>&1', $output, $resultCode);

            if ($resultCode == 0) {
                if (is_array($config['installedpackages'][self::CONFIGPREFIX . 'bindzone'])) {
                    $bindzone = $config['installedpackages'][self::CONFIGPREFIX . 'bindzone'];
                    for ($x = 0; $x < sizeof($bindzone); $x++) {
                        $zone = $config['installedpackages'][ZoneParser::CONFIGPREFIX . 'bindzone']['config'][$x];
                        if ($zone['view'] == $view && $zone['name'] == $rzone && $zone['type'] == $zonetype) {
                            if ($zone['resultconfig'] != $data) {
                                $zone['resultconfig'] = $data;
                                write_config("sxbind: Saved resulting config file for zone " . $zonename . " in xml");
                            }
                        }
                    }
                }
            }

            unlink($tempDB);

            return [
                'result_code' => $resultCode,
                'message' => implode("\n", $output),
                'success' => ($resultCode == 0 ? true : false)
            ];
        }

        /**
         * Returns the complete Zone Dump including all Zones, served by BIND. This includes all master, slave and automatically created Zones.
         * @return array|bool Returns array of zone records or false if failed.
         */
        public static function get_rndc_zone_dump_parsed()
        {
            return self::parse_rndc_zone_dump(self::get_rndc_zone_dump());
        }

        public static function create_tsig_key($name)
        {
            exec('tsig-keygen ' . escapeshellcmd($name) . ' 2>&1', $output, $retval);
            return implode("\n", $output);
        }

        /**
         * Returns the complete Zone Dump including all Zones, served by BIND. This includes all master, slave and automatically created Zones.
         * @return string|bool Returns the complete Zone Dump as string or false if failed.
         */
        public static function get_rndc_zone_dump()
        {
            $rndc = self::CMD_RNDC;

            $output = null;
            $retval = null;

            $lock = lock("named_dump");
            try {
                // Make sure the zone file is recent.
                exec("{$rndc} sync -clean " . ' 2>&1', $output, $retval);
                if ($retval !== 0) {
                    throw new \Exception('Exception during sync. Code:' . $retval . "\n" . implode("\n", $output));
                }

                // Dump all zone data to file
                exec("{$rndc} dumpdb -zones" . ' 2>&1', $output, $retval);
                if ($retval !== 0) {
                    throw new \Exception('Exception during zone compiling. Code:' . $retval . "\n" . implode("\n", $output));
                }

                $dumpFile = self::PATH_DUMPFILE;

                if (self::waitfor_string_in_file($dumpFile, "; Dump complete", 30)) {
                    return file_get_contents($dumpFile);
                }
            } finally {
                unlock($lock);
            }
            return false;
        }

        /**
         * Compiles the Bind DNS Zone to a human readable and parseable string.
         * @param string $view View of the Zone
         * @param string $zone The full zone name
         * @param string $type Optional Zone-Type. default 'master'
         * @throws \Exception Throws an exception if failed.
         * @return string Returns parsed and normalized zone DB as string.
         */
        public static function compilezone($view, $zone, $type = 'master')
        {
            $revname = self::re_reverse_zonename($zone);
            $zoneFile = CHROOT_LOCALBASE . "/etc/namedb/{$type}/{$view}/{$revname}.DB";
            if (!file_exists($zoneFile)) {
                $zoneFile = CHROOT_LOCALBASE . "/etc/namedb/slave/{$view}/{$revname}.DB";
                if (!file_exists($zoneFile)) {
                    throw new \Exception('Zone file not found: ' . $zoneFile);
                }
            }

            $tmpfname = tempnam("/tmp", "zoneConvert");
            $output = null;
            $retval = null;

            exec(self::$PATH_COMPILEZONE . ' -j -f text -F text -q -i none -s full -o ' . escapeshellarg($tmpfname) . " " . escapeshellarg($zone) . " " . escapeshellarg($zoneFile) . ' 2>&1', $output, $retval);
            if ($retval != 0) {
                exec(self::$PATH_COMPILEZONE . ' -j -f raw -F text -q -i none -s full -o ' . escapeshellarg($tmpfname) . " " . escapeshellarg($zone) . " " . escapeshellarg($zoneFile) . ' 2>&1', $output, $retval);
                if ($retval !== 0) {
                    throw new \Exception('Exception during zone compiling. Code:' . $retval . " \n" . implode("\n", $output));
                }
            }

            $result = file_get_contents($tmpfname);
            unlink($tmpfname);

            return $result;
        }

        /**
         * Adds and removes records from zone db.
         * @param mixed $zone
         * @param mixed $view
         * @param mixed $items_add
         * @param mixed $items_remove
         * @param mixed $type
         * @param bool  $freeze_thaw
         * @return bool
         */
        public static function addremove_items_from_zone($zone, $view, $items_add, $items_remove, $type = 'master', $freeze_thaw = true)
        {
            $rndc = self::CMD_RNDC;
            $changed = false;

            if ($freeze_thaw) {
                self::freeze_zone($view, $zone);
            }

            try {
                $zoneData = self::parse_rndc_zone_dump(self::compilezone($view, $zone));
                $zoneDataNew = [];

                foreach ($zoneData as $record) {
                    if ($record['type'] == 'SOA') {
                        $serial = (int) ($record['serial'] ?: 0);
                        $record['serial'] = $serial + 1;
                    }

                    foreach ($items_remove as $item) {
                        if (
                            $record['name'] == $item['name'] &&
                            $record['rdata'] == $item['rdata'] &&
                            $record['type'] == $item['type']
                        ) {
                            //Skip if removed
                            $changed = true;
                            continue 2;
                        }
                    }

                    array_push($zoneDataNew, self::zone_record_to_string($record));
                }

                foreach ($items_add as $item) {
                    array_push($zoneDataNew, self::zone_record_to_string($item));
                    $changed = true;
                }

                if ($changed) {
                    self::save_zonefile(implode("\n", $zoneDataNew) . "\n", $type, $view, $zone);
                }
            } catch (\Exception $e) {
                throw $e;

            } finally {
                if ($freeze_thaw) {
                    self::thaw_zone($view, $zone);
                }
                unset($zoneDataNew);
                unset($zoneData);
            }

            return $changed;
        }

        public static function strip_after_string($str, $char)
        {
            $pos = strpos($str, $char);
            if ($pos !== false) {
                return substr($str, 0, $pos);
            } else {
                return $str;
            }
        }

        /**
         * Parse files created with RNDC dumpdb -zones
         * parse the zone file which was exported with "named-compilezone -F text" or "RNDC dumpdb -zones"
         * @param string $value Zone Dump as string
         * @param string $zone  Zone name as reference if no irigin is found in $value
         * @param bool $include_comment_only True to include Datarows which only contains comment. If false, those rows are ignored.
         * @return array Returns array of zone record objects.
         */
        public static function parse_rndc_zone_dump($value, $zone = '', $include_comment_only = false)
        {
            $reg_host = "{(?<name>.+?)?(?:\s+?(?<ttl>\d*))?\s+(?<class>IN)?\s+(?<type>\p{L}+)\s+(?<rdata>.*)}";
            $regzone = "{;.+?\'(?<zone>.+?)\/.+/(?<view>.*)\'}";
            $zone_data_parsed = [];

            if ($value) {
                if ($zone !== '' && !str_ends_with($zone, '.')) {
                    $zone .= '.';
                }

                $view = '';
                $origin = $zone;
                $defaultTTL = '8700';
                $last_name = '';
                $data_rows = [];
                $item_continue = false;
                $index = -1;

                // normalize multi row values
                foreach (explode("\n", $value) as $line) {
                    $index++;
                    if (preg_match($regzone, $line)) {
                        // pass new zone marker.
                        array_push($data_rows, ['raw' => $line]);
                        continue;
                    }

                    // split comments and values
                    $split_comment = self::mb_explode_escaped(';', $line);
                    $line_without_comment = trim($split_comment[0]);

                    // everything after the first ; will be used as comment
                    unset($split_comment[0]);
                    $line_comment = implode(' ', $split_comment);
                    if (!$item_continue) {
                        // detect multiline start
                        $split_multiline = self::mb_explode_escaped('(', $line);
                        if (count($split_multiline) > 1) {
                            $item_continue = [
                                'comment' => $line_comment,
                                'raw' => implode('', $split_multiline),
                                'index' => $index
                            ];
                            continue;
                        } else {
                            $item = [
                                'comment' => $line_comment,
                                'raw' => $line_without_comment,
                                'index' => $index,
                                'rowcount' => 1
                            ];
                        }
                    } else {
                        // detect multiline end
                        $split_multiline = self::mb_explode_escaped(')', $line);
                        if (count($split_multiline) > 1) {
                            $item_continue['raw'] .= implode('', $split_multiline);
                            $item_continue['rowcount'] = $index - $item_continue['index'] + 1;
                            $item = $item_continue;
                            $item_continue = false;
                        } else {
                            $item_continue['comment'] .= ' ' . $line_comment;
                            $item_continue['raw'] .= ' ' . $line_without_comment;
                            continue;
                        }
                    }

                    $item['raw'] = trim($item['raw']);
                    $item['comment'] = trim($item['comment']);
                    array_push($data_rows, $item);

                }
                // process zone records
                foreach ($data_rows as $data_row) {
                    if ((empty($data_row['raw']) || trim($data_row['raw']) == '') && (empty($data_row['comment']) || trim($data_row['comment']) == '')) {
                        // empty row
                    } elseif ($include_comment_only && (empty($data_row['raw']) || trim($data_row['raw']) == '')) {
                        // comment only
                        $record = [
                            'zone' => $zone,
                            'view' => $view,
                            '_index' => $data_row['index'],
                            '_rowcount' => $data_row['rowcount'],
                            'comment' => $data_row['comment'],
                            'name' => '',
                            'ttl' => '',
                            'type' => ';',
                            'rdata' => $data_row['comment'],
                            'class' => ''
                        ];
                        array_push($zone_data_parsed, $record);

                    } elseif (preg_match('{\$TTL\s+(?<ttl>\d+\w?)\s*}', $data_row['raw'], $matches)) {
                        // find @TTL
                        $defaultTTL = $matches['ttl'];
                        $last_name = '';

                    } elseif (preg_match('{\$ORIGIN\s+(?<origin>\S+)\s*}', $data_row['raw'], $matches)) {
                        // find @ORIGIN
                        $origin = $matches['origin'];
                        $last_name = '';

                    } elseif (preg_match($regzone, $data_row['raw'], $matches)) {
                        // find ZONE NAME in BIND Dump
                        $zone = $matches['zone'] . '.';
                        $view = $matches['view'];
                        $origin = $zone;
                        $last_name = '';

                    } elseif (preg_match($reg_host, $data_row['raw'], $matches)) {
                        // regular zone record
                        $record = [
                            'zone' => $zone,
                            'view' => $view,
                            '_index' => $data_row['index'],
                            '_rowcount' => $data_row['rowcount'],
                            'comment' => $data_row['comment'],
                            'name' => $matches['name'],
                            'ttl' => $matches['ttl'],
                            'type' => strtoupper($matches['type']),
                            'rdata' => $matches['rdata'],
                            'class' => strtoupper($matches['class'])
                        ];

                        if (!$record['name']) {
                            $record['name'] = $last_name;
                        }
                        if (!$record['ttl']) {
                            $record['ttl'] = $defaultTTL;
                        }

                        // convert name to FQDN
                        if ($record['name'] == '@' || $record['name'] == '.') {
                            $record['name'] = $origin;
                        } elseif (!str_ends_with($record['name'], '.')) {
                            $record['name'] = $record['name'] . ".{$origin}";
                        }

                        // split host. only for display
                        $record['name_part1'] = $record['name'];
                        $a = strripos($record['name'], ".{$zone}");
                        if (strtolower($record['name']) == strtolower("{$zone}")) {
                            $record['name_part1'] = $record['name'];
                            $record['name_part2'] = '';
                        } elseif ($a > 0) {
                            $record['name_part1'] = substr($record['name'], 0, $a);
                            $record['name_part2'] = ".{$zone}";
                        }

                        $record['_id'] = self::get_record_id($record);
                        self::expand_zone_record($record);

                        // remember name if next record has no name
                        $last_name = $record['name'];
                        array_push($zone_data_parsed, $record);
                    }
                }
            }

            return $zone_data_parsed;
        }

        public static function get_record_id($object)
        {
            if (empty($object['_id'])) {
                return md5($object['name'] . '/' . $object['type'] . '/' . $object['rdata']);
            } else {
                return $object['_id'];
            }
        }

        public static function record_exists_by_name($entries, $name, $types = ['A', 'AAAA', 'PTR'])
        {
            foreach ($entries as $entry) {
                if (trim($entry['name'], '.') == trim($name, '.') && in_array($entry['type'], $types)) {
                    return true;
                }
            }
            return false;
        }

        /* unescape zone record. 
        TODO: Maybe there are additional escape rules? */
        public static function escape_dns_string($val)
        {
            $search = ['\\', '"', ';'];
            $replace = ['\\\\', '\\"', '\\;'];
            return '"' . str_replace($search, $replace, $val) . '"';
        }

        // similar to php explode() but the delimiter can be escaped. Additionally quoted text can be extracted.
        public static function unescape_dns_string($val)
        {
            $search = ['\\;', '\\"', '\\\\'];
            $replace = [';', '"', '\\'];

            $ret = str_replace($search, $replace, $val);
            return $ret;
        }
        public static function mb_explode_escaped($delimiter, $str, $trim_outer_quote_whitespace = true, $escapeChar = '\\', $quoteChar = '"', $encoding = 'UTF-8')
        {
            $split = [];
            $index = 0;
            $in_quotes = false;
            $is_escaped = false;
            $was_in_quotes = false;
            $split[$index] = '';
            $whitespaces = [' ', "\n", "\r", "\t"];

            $length = mb_strlen($str, $encoding);
            for ($x = 0; $x < $length; $x++) {
                $char = mb_substr($str, $x, 1, $encoding);

                // Detect escape char
                if ($char === $escapeChar && !$is_escaped) {
                    $is_escaped = true;
                    continue;
                }

                // detect if in quotes
                if ($char === $quoteChar && !$is_escaped) {
                    $in_quotes = ($in_quotes === false);
                    if ($in_quotes) {
                        $was_in_quotes = true;
                    }
                }

                // detect delimiter
                if ($char === $delimiter && !$is_escaped && (!$in_quotes || $delimiter === $quoteChar)) {
                    $index++;
                    $split[$index] = '';
                    continue;
                }

                // whitespace handling outside quotes.
                if ($trim_outer_quote_whitespace && !$in_quotes && $was_in_quotes && in_array($char, $whitespaces)) {
                    if (in_array($char, $whitespaces)) {
                        continue;
                    } else {
                        $was_in_quotes = false;
                    }
                }

                $split[$index] = $split[$index] . $char;
                $is_escaped = false;
            }

            return $split;
        }

        // expand rdata to individual fields
        public static function expand_zone_record(&$record)
        {
            // parse rdata
            $val = preg_split("/[\s,]*\\\"([^\\\"]+)\\\"[\s,]*|" .
                "[\s,]*'([^']+)'[\s,]*|" .
                "[\s,]+/", $record['rdata'], 0, PREG_SPLIT_NO_EMPTY | PREG_SPLIT_DELIM_CAPTURE);

            switch ($record['type']) {
                case 'MX':
                    $record['_required'] = ['name', 'ttl', 'priority', 'host'];
                    if (count($val) == 2) {
                        $record['priority'] = $val[0];
                        $record['host'] = $val[1];
                        $record['_expanded'] = true;
                    } else {
                        $record['priority'] = '';
                        $record['host'] = '';
                        $record['_expanded'] = false;
                    }
                    break;

                case 'SRV':
                    $record['_required'] = ['name', 'ttl', 'priority', 'weight', 'port', 'host'];
                    if (count($val) == 4) {
                        $record['priority'] = $val[0];
                        $record['weight'] = $val[1];
                        $record['port'] = $val[2];
                        $record['host'] = $val[3];
                        $record['_expanded'] = true;
                    } else {
                        $record['priority'] = '';
                        $record['weight'] = '';
                        $record['port'] = '';
                        $record['host'] = '';
                        $record['_expanded'] = false;
                    }
                    break;

                case 'NS':
                    $record['_required'] = ['name', 'ttl', 'nameserver'];
                    if (count($val) == 1) {
                        $record['nameserver'] = $val[0];
                        $record['_expanded'] = true;
                    } else {
                        $record['nameserver'] = '';
                        $record['_expanded'] = false;
                    }
                    break;

                case 'PTR':
                    $record['_required'] = ['name', 'ttl', 'host'];
                    if (count($val) == 1) {
                        $record['ip'] = self::ptr_to_ip($record['name']);
                        $record['host'] = $val[0];
                        $record['_expanded'] = true;
                    } else {
                        $record['host'] = $val[0];
                        $record['_expanded'] = false;
                    }
                    break;

                case 'A':
                case 'AAAA':
                    $record['_required'] = ['name', 'ttl', 'ip'];
                    if (count($val) == 1) {
                        $record['ip'] = $val[0];
                        $record['ptr'] = self::ip_to_ptr($record['ip']);
                        $record['_expanded'] = true;
                    } else {
                        $record['ip'] = '';
                        $record['_expanded'] = true;
                    }
                    break;

                case 'TXT':
                case 'SPF':
                    $record['_required'] = ['name', 'ttl', 'txt'];
                    $record['txt'] = self::unescape_dns_string(implode('', self::mb_explode_escaped('"', $record['rdata'])));
                    $record['_expanded'] = true;
                    break;

                case 'CNAME':
                    $record['_required'] = ['name', 'ttl', 'host'];
                    if (count($val) == 1) {
                        $record['host'] = $val[0];
                        $record['_expanded'] = true;
                    } else {
                        $record['host'] = '';
                        $record['_expanded'] = false;
                    }
                    break;

                case 'SOA':
                    $record['_required'] = ['name', 'ttl', 'mname', 'rname', 'serial', 'refresh', 'retry', 'expire', 'minimum'];
                    if (count($val) == 7) {
                        $record['mname'] = $val[0];
                        $record['rname'] = $val[1];
                        $record['serial'] = $val[2];
                        $record['refresh'] = $val[3];
                        $record['retry'] = $val[4];
                        $record['expire'] = $val[5];
                        $record['minimum'] = $val[6];
                        $record['_expanded'] = true;
                    } else {
                        $record['mname'] = '';
                        $record['rname'] = '';
                        $record['serial'] = '';
                        $record['refresh'] = '';
                        $record['retry'] = '';
                        $record['expire'] = '';
                        $record['minimum'] = '';
                        $record['_expanded'] = false;
                    }
                    break;

                default:
                    $record['_required'] = ['name', 'ttl', 'rdata'];
            }
        }

        // merge individual values to rdata
        public static function collapse_zone_record(&$record)
        {
            switch ($record['type']) {
                case 'MX':
                    $record['rdata'] = $record['priority'] . ' ' . $record['host'];
                    break;

                case 'SRV':
                    $record['rdata'] = $record['priority'] . ' ' . $record['weight'] . ' ' . $record['port'] . ' ' . $record['host'];
                    break;

                case 'NS':
                    $record['rdata'] = $record['nameserver'];
                    break;

                case 'PTR':
                    $record['rdata'] = $record['host'];
                    break;

                case 'A':
                case 'AAAA':
                    $record['rdata'] = $record['ip'];
                    break;

                case 'TXT':
                case 'SPF':
                    // escape string and split by 127 chars... aprox 255 byte.
                    $str = mb_str_split(self::escape_dns_string($record['txt']), 127, 'UTF-8');
                    $record['rdata'] = implode('" "', array_filter($str, 'strlen'));
                    break;

                case 'CNAME':
                    $record['rdata'] = $record['host'];
                    break;

                case 'SOA':
                    $record['rdata'] = $record['mname'] .
                        ' ' . $record['rname'] .
                        ' ' . $record['serial'] .
                        ' ' . $record['refresh'] .
                        ' ' . $record['retry'] .
                        ' ' . $record['expire'] .
                        ' ' . $record['minimum'];
                    break;
            }
        }
        /**
         * Returns the Zone Data as an array of objects.
         * @param string $zonetype
         * @param string $zoneview
         * @param string $zonename The full zonename.
         * @return array|null
         */
        public static function get_zone_dump_parsed($zonetype, $zoneview, $zonename)
        {
            $rndc = self::CMD_RNDC;
            $zonename = rtrim($zonename, '.');
            $zonename_reverse = self::re_reverse_zonename($zonename);

            $temp_zone_file = tempnam('/tmp', "{$zonetype}_{$zoneview}_{$zonename_reverse}");
            $zonefile = CHROOT_LOCALBASE . "/etc/namedb/{$zonetype}/{$zoneview}/{$zonename_reverse}.DB";
            $current_zone_data_parsed = null;

            // Make sure the zone file is recent.
            // Not required anymore cause "named-compilezone -j"  reads journal file too.
            //exec("{$rndc} sync -clean " . escapeshellarg($zonename) . " IN " . escapeshellarg($zoneview));

            // TODO: Check if there is a better way to detect if zone is raw or text.
            exec(self::$PATH_COMPILEZONE . ' -j -f text -F text -i none -s full ' .
                ' -o ' . escapeshellarg($temp_zone_file) . ' ' .
                escapeshellarg($zonename) . ' ' .
                escapeshellarg($zonefile) . ' 2>&1', $output, $resultCode);

            if ($resultCode != 0) {
                exec(self::$PATH_COMPILEZONE . ' -j -f raw -F text -i none -k ignore -m ignore -M ignore -S ignore -W ignore -s full ' .
                    ' -o ' . escapeshellarg($temp_zone_file) . ' ' .
                    escapeshellarg($zonename) . ' ' .
                    escapeshellarg($zonefile) . ' 2>&1', $output, $resultCode);
            }

            if ($resultCode == 0) {
                $current_zone_data = file_get_contents($temp_zone_file);
                $current_zone_data_parsed = self::parse_rndc_zone_dump($current_zone_data, $zonename_reverse);
                unlink($temp_zone_file);
            } else {
                $error = "[bind] READ FAILED - Zone {$zonename_reverse} has lost dynamic entries.\n" . implode("\n", $output);
                log_error($error);
            }

            return $current_zone_data_parsed;
        }

        // convert single zone record to string. Use collapse_zone_record() to update rdata first.
        public static function zone_record_to_string($record)
        {
            return ($record['name'] ?: ' ') . "\t" .
                ($record['ttl'] ?: ' ') .
                ' IN ' .
                ($record['type'] ?: ' ') . "\t" .
                ($record['rdata'] ?: ' ');
        }

        // convert IPv4 or IPv6 to it's PTR string
        public static function ip_to_ptr($ip)
        {
            $ipstring = trim($ip);
            // remove Subnet if exists.
            $cx = strpos($ipstring, '/');
            if ($cx) {
                $subnet = (int) (substr($ipstring, $cx + 1));
                $ipstring = substr($ipstring, 0, $cx);
            } else {
                $subnet = null; // No netmask present
            }

            if (str_contains($ipstring, ':')) {
                //calculate subnet from incomlete IPv6 and complete ip wwith "0"
                $length = substr_count($ipstring, ':');
                if (!str_contains($ipstring, '::') && $length < 7) {
                    if (str_ends_with($ipstring, ':')) {
                        $ipstring = $ipstring . ':';
                    } else {
                        $ipstring = $ipstring . '::';
                    }
                    if ($subnet == null) {
                        $subnet = ($length + 1) * 16;
                    }
                } else {
                    if ($subnet == null) {
                        $subnet = 128;
                    }
                }

                // get reverse IPv6
                $unpack = unpack('H*hex', inet_pton($ipstring));
                $hex = $unpack['hex'];
                $iparr = array_reverse(str_split($hex));
                if ($subnet < 128) {
                    $iparr = array_slice($iparr, intval(32 - floor($subnet / 4)));
                }
                $result = implode('.', $iparr) . '.ip6.arpa';
            } else {
                //calculate subnet from incomlete IPv4 and complete ip wwith "0"
                $ipstring = trim($ipstring, '.');
                $length = substr_count($ipstring, '.');
                if ($length < 3) {
                    for ($i = $length; $i < 3; $i++) {
                        $ipstring = $ipstring . '.0';
                    }
                    if ($subnet == null) {
                        $subnet = ($length + 1) * 8;
                    }
                } else {
                    if ($subnet == null) {
                        $subnet = 32;
                    }
                }

                // get reverse IPv4
                $iparr = array_reverse(explode(".", $ipstring));
                if ($subnet < 32) {
                    $iparr = array_slice($iparr, intval(4 - floor($subnet / 8)));
                }
                $addr = implode('.', $iparr);
                $result = $addr . '.in-addr.arpa';
            }

            return $result;
        }

        // convert IPv4 or IPv6 from PTR string to it's IP address
        public static function ptr_to_ip($ptr)
        {
            $ptr = rtrim(trim($ptr), ".");

            if (str_ends_with($ptr, '.in-addr.arpa')) {
                $addr = explode(".", substr($ptr, 0, -13));
                return implode('.', array_reverse($addr));

            } elseif (str_ends_with($ptr, '.ip6.arpa')) {
                $mainptr = substr($ptr, 0, -9);
                $pieces = array_reverse(explode(".", $mainptr));
                $hex = implode("", $pieces);
                $ipbin = pack('H*', $hex);
                return inet_ntop($ipbin);
            }
        }

        // compares two parsed zones and returns the difference
        public static function diff_zonerecords($zone1, $zone2)
        {
            $diff = [];
            foreach ($zone1 as $a) {
                $match = false;
                foreach ($zone2 as $b) {
                    if (
                        strtolower($a['name']) == strtolower($b['name']) &&
                        strtolower($a['rdata']) == strtolower($b['rdata']) &&
                        strtolower($a['type']) == strtolower($b['type'])
                    ) {
                        $match = true;
                        break;
                    }
                }
                if (!$match) {
                    array_push($diff, $a);
                }
            }
            return $diff;
        }

        /** 
         * Changes the serial in the given zone config.
         * @param string $zone_conf The content of the zone DB.
         * @param string $new_zoneserial New Serial to set
         * @return string The new zone DB as string.
         */
        public static function set_serial_zoneconf($zone_conf, $new_zoneserial)
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
                log_error('[bind] Serial in zone_conf not found!');
            }
            return $zone_conf;
        }

        public static function waitfor_string_in_file($filename, $string, $timeout)
        {
            $start = $now = time();

            while (($now - $start) < $timeout) {
                $testfile = @fopen($filename, "r");

                if ($testfile != FALSE) {
                    while (($line = fgets($testfile)) !== false) {
                        if (strpos($line, $string) !== false) {
                            fclose($testfile);
                            return (true);
                        }
                    }
                    fclose($testfile);
                }

                sleep(1);
                $now = time();
            }
            throw new \Exception('Timeout while waiting for end of file');
        }

        /**
         * Reads the ZoneList from the bindzone config in pfSense config file.
         * Sorted alphabetically 
         */
        public static function get_zonelist()
        {
            global $config;

            if (is_array($config['installedpackages'][self::CONFIGPREFIX . 'bindzone'])) {
                $bindzone = $config['installedpackages'][self::CONFIGPREFIX . 'bindzone']['config'];
                $col = array_column($bindzone, 'name');
                array_multisort($col, SORT_ASC, $bindzone);
            } else {
                $bindzone = array();
            }
            return $bindzone;
        }
        /**
         * Reads all static zone records from config.xml for all Zones.
         * @param boolean $include_customzonerecords If true, custom zone records will also be included in the result. 
         * '_iscustomrecord' will be set to true for all records found in custom zone file. Otherwise '_iscustomrecord' is false or undefined.
         * @return array Returns array of zone records as it would be returned from parse_rndc_zone_dump().
         */
        public static function get_zonerecords($include_customzonerecords = true)
        {
            global $config;
            $ret = [];

            foreach (self::get_zonelist() as $zone) {
                $zone_name = self::reverse_zonename($zone) . '.';
                $zone_conf = '';

                foreach ($zone['row'] as $row) {
                    $hosttype = $row['hosttype'];
                    $hostname = $row['hostname'];

                    if (preg_match("/(MX|NS)/", $row['hosttype']) && ($hostname == "")) {
                        $hostname = $zone_name;
                    }

                    $hostdst = $row['hostdst'];
                    if (preg_match("/[a-zA-Z]/", $hostdst) && !preg_match("/(TXT|SPF|AAAA)/", $hosttype)) {
                        $hostdst .= ".";
                    }
                    if (preg_match("/(TXT|SPF)/", $hosttype)) {
                        if (!str_starts_with(trim($hostdst), '"')) {
                            $str = mb_str_split(bind_escape_dns_string($hostdst), 127, 'UTF-8');
                            $hostdst = implode('" "', array_filter($str, 'strlen'));
                        }
                    }

                    $hostvalue = $row['hostvalue'];
                    $zone_conf .= "{$hostname} IN {$hosttype} {$hostvalue} {$hostdst}\n";
                }

                $index_customrecords = count(explode("\n", $zone_conf));
                if ($include_customzonerecords && $zone['customzonerecords'] != "") {
                    $zone_conf .= base64_decode($zone['customzonerecords']) . "\n";
                }

                $parsed = self::parse_rndc_zone_dump($zone_conf, $zone_name, false);

                if ($include_customzonerecords && $zone['customzonerecords'] != "") {
                    foreach ($parsed as &$row) {
                        $row['_iscustomrecord'] = ($row['_index'] >= $index_customrecords);
                    }
                }
                $ret = array_merge($ret, $parsed);
            }

            return $ret;
        }

        /**
         * Reads all Zones from Zonedump
         * @param array $zonedump_parsed Data from the zone dump file
         * @return array Zone list
         */
        public static function get_all_zones_from_dump($zonedump_parsed)
        {
            $result = [];
            $list = [];
            foreach ($zonedump_parsed as $record) {
                if (!in_array($record['zone'] . '__' . $record['view'], $list)) {
                    $list[] = $record['zone'] . '__' . $record['view'];
                    array_push(
                        $result,
                        [
                            'name' => $record['zone'],
                            'view' => $record['view']
                        ]
                    );
                }

            }
            return $result;
        }

        /**
         * Compares record1 and record2. name, zone and type are case insensitive. rdata is case sensitive. Other parameters like TTL are ignored.
         * @return boolean Returns true if both records match.
         */
        public static function equals_zonerecord($record1, $record2)
        {
            if (
                strtolower($record1['name']) == strtolower($record2['name']) &&
                $record1['rdata'] == $record2['rdata'] &&
                strtoupper($record1['type']) == strtoupper($record2['type']) &&
                strtolower($record1['zone']) == strtolower($record2['zone'])
            ) {
                return true;
            }
            return false;
        }

        /** 
         * Returns the simple zone name by removing .ip6.arpa or .in-addr.arpa (may include . at the end or not) from the string.
         */
        public static function re_reverse_zonename($name)
        {
            if (str_ends_with($name, '.ip6.arpa')) {
                return substr($name, 0, -9);
            }
            if (str_ends_with($name, '.in-addr.arpa')) {
                return substr($name, 0, -13);
            }
            if (str_ends_with($name, '.ip6.arpa.')) {
                return substr($name, 0, -10);
            }
            if (str_ends_with($name, '.in-addr.arpa.')) {
                return substr($name, 0, -14);
            }
            return $name;
        }

        /** 
         * Returns the zone name including ipv6.arpa or in-addr.arpa if required. Otherwise it returns the plain zone name.
         * Be carefull as this does NOT include trailing point (.)
         * 
         * @param mixed $zone A single zone from get_zonelist()
         * @return string Returns the full zone name.
         */
        public static function reverse_zonename($zone)
        {
            if ($zone['reverso'] == "on") {
                if ($zone['reversv6o'] == "on") {
                    return $zone['name'] . ".ip6.arpa";
                } else {
                    return $zone['name'] . ".in-addr.arpa";
                }
            } else {
                return $zone['name'];
            }
        }
    }


    if (file_exists('/usr/local/sbin/named-checkzone')) {
        ZoneParser::$PATH_CHECKZONE = '/usr/local/sbin/named-checkzone';
    } else {
        ZoneParser::$PATH_CHECKZONE = '/usr/local/bin/named-checkzone';
    }

    if (file_exists('/usr/local/sbin/named-compilezone')) {
        ZoneParser::$PATH_COMPILEZONE = '/usr/local/sbin/named-compilezone';
    } else {
        ZoneParser::$PATH_COMPILEZONE = '/usr/local/bin/named-compilezone';
    }
}
?>
