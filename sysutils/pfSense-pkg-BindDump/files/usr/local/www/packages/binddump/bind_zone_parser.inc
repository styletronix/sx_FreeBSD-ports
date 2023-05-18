<?php
class BindZoneParser
{
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
            $index = 0;

            // normalize multi row values
            foreach (explode("\n", $value) as $line) {
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
                $line_comment = implode(';', $split_comment);
                if (!$item_continue) {
                    // detect multiline start
                    $split_multiline = self::mb_explode_escaped('(', $line);
                    if (count($split_multiline) > 1) {
                        $item_continue = [
                            'comment' => $line_comment,
                            'raw' => implode('', $split_multiline),
                            'index' => $index,
                            'rowcount' => 1
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
                $index++;
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
                        'index' => $data_row['index'],
                        'rowcount' => $data_row['rowcount'],
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
                        'index' => $data_row['index'],
                        'rowcount' => $data_row['rowcount'],
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
                    $a = strripos($record['name'], ".{$origin}.");
                    if (strtolower($record['name']) == strtolower("{$origin}.")) {
                        $record['name_part1'] = $record['name'];
                        $record['name_part2'] = '';
                    } elseif ($a > 0) {
                        $record['name_part1'] = substr($record['name'], 0, $a);
                        $record['name_part2'] = ".{$origin}.";
                    }

                    self::expand_zone_record($record);
                    // remember name if next record has no name
                    $last_name = $record['name'];
                    array_push($zone_data_parsed, $record);
                }
            }
        }

        return $zone_data_parsed;
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

    // unescape zone record. 
    // TODO: Maybe there are additional escape rules?
    public static function unescape_dns_string($val)
    {
        $search = ['\\;', '\\"', '\\\\'];
        $replace = [';', '"', '\\'];

        $ret = str_replace($search, $replace, $val);
        return $ret;
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
                if (count($val) == 2) {
                    $record['priority'] = $val[0];
                    $record['host'] = $val[1];
                    $record['_expanded'] = true;
                }
                break;

            case 'SRV':
                if (count($val) == 4) {
                    $record['priority'] = $val[0];
                    $record['weight'] = $val[1];
                    $record['port'] = $val[2];
                    $record['host'] = $val[3];
                    $record['_expanded'] = true;
                }
                break;

            case 'NS':
                if (count($val) == 1) {
                    $record['nameserver'] = $val[0];
                    $record['_expanded'] = true;
                }
                break;

            case 'PTR':
                if (count($val) == 1) {
                    $record['ip'] = self::ptr_to_ip($record['name']);
                    $record['host'] = $val[0];
                    $record['_expanded'] = true;
                }
                break;

            case 'A':
            case 'AAAA':
                if (count($val) == 1) {
                    $record['ip'] = $val[0];
                    $record['ptr'] = self::ip_to_ptr($record['ip']);
                    $record['host'] = $record['name'];
                    $record['_expanded'] = true;
                }
                break;

            case 'TXT':
            case 'SPF':
                $record['txt'] = self::unescape_dns_string(implode('', self::mb_explode_escaped('"', $record['rdata'])));
                $record['_expanded'] = true;
                break;

            case 'CNAME':
                if (count($val) == 1) {
                    $record['host'] = $val[0];
                    $record['_expanded'] = true;
                }
                break;

            case 'SOA':
                if (count($val) == 7) {
                    $record['mname'] = $val[0];
                    $record['rname'] = $val[1];
                    $record['serial'] = $val[2];
                    $record['refresh'] = $val[3];
                    $record['retry'] = $val[4];
                    $record['expire'] = $val[5];
                    $record['minimum'] = $val[6];
                    $record['_expanded'] = true;
                }
                break;
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

    // read and parse current zone db
    public static function get_zone_dump_parsed($zonetype, $zoneview, $zonename, $zonename_reverse)
    {
        $temp_zone_file = "/tmp/nameddump_{$zonetype}_{$zoneview}_{$zonename}.txt";
        $zonefile = CHROOT_LOCALBASE . "/etc/namedb/{$zonetype}/{$zoneview}/{$zonename}.DB";
        $current_zone_data_parsed = null;

        exec('/usr/local/sbin/named-compilezone -F text -i none -s full ' .
            ' -o ' . escapeshellarg($temp_zone_file) . ' ' .
            escapeshellarg($zonename_reverse) . ' ' .
            escapeshellarg($zonefile) . ' 2>&1', $output, $resultCode);

        if ($resultCode == 0) {
            $current_zone_data = file_get_contents($temp_zone_file);
            $current_zone_data_parsed = self::parse_rndc_zone_dump($current_zone_data, $zonename_reverse);
            unlink($temp_zone_file);
        } else {
            $error = "[bind] READ FAILED - Zone {$zonename_reverse} has lost dynamic entries.\n" . implode("\n", $output);
            //file_notice("named_config", $error, "BIND DNS", "", 2);
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

        if (str_contains($ipstring, ':')) {
            $unpack = unpack('H*hex', inet_pton($ipstring));
            $hex = $unpack['hex'];
            return implode('.', array_reverse(str_split($hex))) . '.ip6.arpa';
        } else {
            $addr = implode('.', array_reverse(explode(".", $ipstring)));
            return $addr . '.in-addr.arpa';
        }
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
}
?>
