<?php

/* make sure no-one can run anything here if they didn't arrive through 'proper channels' */
if(!defined("COMPACTCMS_CODE")) { die('Illegal entry point!'); } /*MARKER*/

/*
 * Script: Tooling.php
 *   MooTools FileManager - Backend for the FileManager Script - Support Code
 *
 * Authors:
 *  - Christoph Pojer (http://cpojer.net) (author)
 *  - James Ehly (http://www.devtrench.com)
 *  - Fabian Vogelsteller (http://frozeman.de)
 *  - Ger Hobbelt (http://hebbut.net)
 *
 * License:
 *   MIT-style license.
 *
 * Copyright:
 *   Copyright (c) 2011 [Christoph Pojer](http://cpojer.net)
 */




if (!function_exists('safe_glob'))
{
	/**#@+
	 * Extra GLOB constant for safe_glob()
	 */
	if (!defined('GLOB_NODIR'))       define('GLOB_NODIR',256);
	if (!defined('GLOB_PATH'))        define('GLOB_PATH',512);
	if (!defined('GLOB_NODOTS'))      define('GLOB_NODOTS',1024);
	if (!defined('GLOB_RECURSE'))     define('GLOB_RECURSE',2048);
	if (!defined('GLOB_NOHIDDEN'))    define('GLOB_NOHIDDEN',4096);
	/**#@-*/


	/**
	 * A safe empowered glob().
	 *
	 * Function glob() is prohibited on some server (probably in safe mode)
	 * (Message "Warning: glob() has been disabled for security reasons in
	 * (script) on line (line)") for security reasons as stated on:
	 * http://seclists.org/fulldisclosure/2005/Sep/0001.html
	 *
	 * safe_glob() intends to replace glob() using readdir() & fnmatch() instead.
	 * Supported flags: GLOB_MARK, GLOB_NOSORT, GLOB_ONLYDIR
	 * Additional flags: GLOB_NODIR, GLOB_PATH, GLOB_NODOTS, GLOB_RECURSE, GLOB_NOHIDDEN
	 * (not original glob() flags)
	 *
	 * @author BigueNique AT yahoo DOT ca
	 * @updates
	 * - 080324 Added support for additional flags: GLOB_NODIR, GLOB_PATH,
	 *   GLOB_NODOTS, GLOB_RECURSE
	 * - [i_a] Added support for GLOB_NOHIDDEN, split output in directories and files subarrays
	 */
	function safe_glob($pattern, $flags = 0)
	{
		$split = explode('/', strtr($pattern, '\\', '/'));
		$mask = array_pop($split);
		$path = implode('/', $split);
		if (($dir = @opendir($path)) !== false)
		{
			$dirs = array();
			$files = array();
			while(($file = readdir($dir)) !== false)
			{
				// HACK/TWEAK: PHP5 and below are completely b0rked when it comes to international filenames   :-(
				//             --> do not show such files/directories in the list as they won't be accessible anyway!
				//
				// The regex charset is limited even within the ASCII range, due to    http://en.wikipedia.org/wiki/Filename#Comparison%5Fof%5Ffile%5Fname%5Flimitations
				// Although the filtered characters here are _possible_ on UNIX file systems, they're severely frowned upon.
				if (preg_match('/[^ -)+-.0-;=@-\[\]-{}~]/', $file))  // filesystem-illegal characters are not part of the set:   * > < ? / \ |
				{
					// simply do NOT list anything that we cannot cope with.
					// That includes clearly inaccessible files (and paths) with non-ASCII characters:
					// PHP5 and below are a real mess when it comes to handling Unicode filesystems
					// (see the php.net site too: readdir / glob / etc. user comments and the official
					// notice that PHP will support filesystem UTF-8/Unicode only when PHP6 is released.
					//
					// Big, fat bummer!
					continue;
				}
				//$temp = unpack("H*",$file);
				//echo 'hexdump of filename = ' . $temp[1] . ' for filename = ' . $file . "<br>\n";

				$filepath = $path . '/' . $file;
				$isdir = is_dir($filepath);

				// Recurse subdirectories (GLOB_RECURSE); speedup: no need to sort the intermediate results
				if (($flags & GLOB_RECURSE) && $isdir && !($file == '.' || $file == '..'))
				{
					$subsect = safe_glob($filepath . '/' . $mask, $flags | GLOB_NOSORT);
					if (is_array($subsect))
					{
						if (!($flags & GLOB_PATH))
						{
							$dirs = array_merge($dirs, array_prepend($subject['dirs'], $file . '/'));
							$files = array_merge($files, array_prepend($subject['files'], $file . '/'));
						}
					}
				}
				// Match file mask
				if (fnmatch($mask, $file))
				{
					if ( ( (!($flags & GLOB_ONLYDIR)) || $isdir )
					  && ( (!($flags & GLOB_NODIR)) || !$isdir )
					  && ( (!($flags & GLOB_NODOTS)) || !($file == '.' || $file == '..') )
					  && ( (!($flags & GLOB_NOHIDDEN)) || ($file[0] != '.' || $file == '..')) )
					{
						if ($isdir)
						{
							$dirs[] = ($flags & GLOB_PATH ? $path . '/' : '') . $file . (($flags & GLOB_MARK) ? '/' : '');
						}
						else
						{
							$files[] = ($flags & GLOB_PATH ? $path . '/' : '') . $file;
						}
					}
				}
			}
			closedir($dir);
			if (!($flags & GLOB_NOSORT))
			{
				sort($dirs);
				sort($files);
			}
			return array('dirs' => $dirs, 'files' => $files);
		}
		else
		{
			return false;
		}
	}
}




// derived from http://nl.php.net/manual/en/function.http-build-query.php#90438
if (!function_exists('http_build_query_ex'))
{
	if (!defined('PHP_QUERY_RFC1738')) define('PHP_QUERY_RFC1738', 1); // encoding is performed per RFC 1738 and the application/x-www-form-urlencoded media type, which implies that spaces are encoded as plus (+) signs.
	if (!defined('PHP_QUERY_RFC3986')) define('PHP_QUERY_RFC3986', 2); // encoding is performed according to » RFC 3986, and spaces will be percent encoded (%20).

	function http_build_query_ex($data, $prefix = '', $sep = '', $key = '', $enc_type = PHP_QUERY_RFC1738)
	{
		$ret = array();
		if (!is_array($data) && !is_object($data))
		{
			if ($enc_type == PHP_QUERY_RFC1738)
			{
				$ret[] = urlencode($data);
			}
			else
			{
				$ret[] = rawurlencode($data);
			}
		}
		else
		{
			if (!empty($prefix))
			{
				if ($enc_type == PHP_QUERY_RFC1738)
				{
					$prefix = urlencode($prefix);
				}
				else
				{
					$prefix = rawurlencode($prefix);
				}
			}
			foreach ($data as $k => $v)
			{
				if (is_int($k))
				{
					$k = $prefix . $k;
				}
				else if ($enc_type == PHP_QUERY_RFC1738)
				{
					$k = urlencode($k);
				}
				else
				{
					$k = rawurlencode($k);
				}
				if (!empty($key) || $key === 0)
				{
					$k = $key . '[' . $k . ']';
				}
				if (is_array($v) || is_object($v))
				{
					$ret[] = http_build_query_ex($v, '', $sep, $k, $enc_type);
				}
				else
				{
					if ($enc_type == PHP_QUERY_RFC1738)
					{
						$v = urlencode($v);
					}
					else
					{
						$v = rawurlencode($v);
					}
					$ret[] = $k . '=' . $v;
				}
			}
		}
		if (empty($sep)) $sep = ini_get('arg_separator.output');
		return implode($sep, $ret);
	}
}



/**
 * Determine how the PHP interpreter was invoked: cli/cgi/fastcgi/server,
 * where 'server' implies PHP is part of a webserver in the form of a 'module' (e.g. mod_php5) or similar.
 *
 * This information is used, for example, to decide the correct way to send the 'respose header code':
 * see send_response_status_header().
 */
if (!function_exists('get_interpreter_invocation_mode'))
{
	function get_interpreter_invocation_mode()
	{
		global $_ENV;
		global $_SERVER;

		/*
		 * see
		 *
		 * http://nl2.php.net/manual/en/function.php-sapi-name.php
		 * http://stackoverflow.com/questions/190759/can-php-detect-if-its-run-from-a-cron-job-or-from-the-command-line
		 */
		$mode = "server";
		$name = php_sapi_name();
		if (preg_match("/fcgi/", $name) == 1)
		{
			$mode = "fastcgi";
		}
		else if (preg_match("/cli/", $name) == 1)
		{
			$mode = "cli";
		}
		else if (preg_match("/cgi/", $name) == 1)
		{
			$mode = "cgi";
		}

		/*
		 * check whether POSIX functions have been compiled/enabled; xampp on Win32/64 doesn't have the buggers! :-(
		 */
		if (function_exists('posix_isatty'))
		{
			if (posix_isatty(STDOUT))
			{
				/* even when seemingly run as cgi/fastcgi, a valid stdout TTY implies an interactive commandline run */
				$mode = 'cli';
			}
		}

		if (!empty($_ENV['TERM']) && empty($_SERVER['REMOTE_ADDR']))
		{
			/* even when seemingly run as cgi/fastcgi, a valid stdout TTY implies an interactive commandline run */
			$mode = 'cli';
		}

		return $mode;
	}
}






/**
 * Return the HTTP response code string for the given response code
 */
if (!function_exists('get_response_code_string'))
{
	function get_response_code_string($response_code)
	{
		$response_code = intval($response_code);
		switch ($response_code)
		{
		case 100:   return "RFC2616 Section 10.1.1: Continue";
		case 101:   return "RFC2616 Section 10.1.2: Switching Protocols";
		case 200:   return "RFC2616 Section 10.2.1: OK";
		case 201:   return "RFC2616 Section 10.2.2: Created";
		case 202:   return "RFC2616 Section 10.2.3: Accepted";
		case 203:   return "RFC2616 Section 10.2.4: Non-Authoritative Information";
		case 204:   return "RFC2616 Section 10.2.5: No Content";
		case 205:   return "RFC2616 Section 10.2.6: Reset Content";
		case 206:   return "RFC2616 Section 10.2.7: Partial Content";
		case 300:   return "RFC2616 Section 10.3.1: Multiple Choices";
		case 301:   return "RFC2616 Section 10.3.2: Moved Permanently";
		case 302:   return "RFC2616 Section 10.3.3: Found";
		case 303:   return "RFC2616 Section 10.3.4: See Other";
		case 304:   return "RFC2616 Section 10.3.5: Not Modified";
		case 305:   return "RFC2616 Section 10.3.6: Use Proxy";
		case 307:   return "RFC2616 Section 10.3.8: Temporary Redirect";
		case 400:   return "RFC2616 Section 10.4.1: Bad Request";
		case 401:   return "RFC2616 Section 10.4.2: Unauthorized";
		case 402:   return "RFC2616 Section 10.4.3: Payment Required";
		case 403:   return "RFC2616 Section 10.4.4: Forbidden";
		case 404:   return "RFC2616 Section 10.4.5: Not Found";
		case 405:   return "RFC2616 Section 10.4.6: Method Not Allowed";
		case 406:   return "RFC2616 Section 10.4.7: Not Acceptable";
		case 407:   return "RFC2616 Section 10.4.8: Proxy Authentication Required";
		case 408:   return "RFC2616 Section 10.4.9: Request Time-out";
		case 409:   return "RFC2616 Section 10.4.10: Conflict";
		case 410:   return "RFC2616 Section 10.4.11: Gone";
		case 411:   return "RFC2616 Section 10.4.12: Length Required";
		case 412:   return "RFC2616 Section 10.4.13: Precondition Failed";
		case 413:   return "RFC2616 Section 10.4.14: Request Entity Too Large";
		case 414:   return "RFC2616 Section 10.4.15: Request-URI Too Large";
		case 415:   return "RFC2616 Section 10.4.16: Unsupported Media Type";
		case 416:   return "RFC2616 Section 10.4.17: Requested range not satisfiable";
		case 417:   return "RFC2616 Section 10.4.18: Expectation Failed";
		case 500:   return "RFC2616 Section 10.5.1: Internal Server Error";
		case 501:   return "RFC2616 Section 10.5.2: Not Implemented";
		case 502:   return "RFC2616 Section 10.5.3: Bad Gateway";
		case 503:   return "RFC2616 Section 10.5.4: Service Unavailable";
		case 504:   return "RFC2616 Section 10.5.5: Gateway Time-out";
		case 505:   return "RFC2616 Section 10.5.6: HTTP Version not supported";
	/*
		case 102:   return "Processing";  // http://www.askapache.com/htaccess/apache-status-code-headers-errordocument.html#m0-askapache3
		case 207:   return "Multi-Status";
		case 418:   return "I'm a teapot";
		case 419:   return "unused";
		case 420:   return "unused";
		case 421:   return "unused";
		case 422:   return "Unproccessable entity";
		case 423:   return "Locked";
		case 424:   return "Failed Dependency";
		case 425:   return "Node code";
		case 426:   return "Upgrade Required";
		case 506:   return "Variant Also Negotiates";
		case 507:   return "Insufficient Storage";
		case 508:   return "unused";
		case 509:   return "unused";
		case 510:   return "Not Extended";
	*/
		default:   return rtrim("Unknown Response Code " . $response_code);
		}
	}
}



/**
 * Performs the correct way of transmitting the response status code header: PHP header() must be invoked in different ways
 * dependent on the way the PHP interpreter has been invoked.
 *
 * See also:
 *
 *   http://nl2.php.net/manual/en/function.header.php
 */
if (!function_exists('send_response_status_header'))
{
	function send_response_status_header($response_code)
	{
		$mode = get_interpreter_invocation_mode();
		switch ($mode)
		{
		default:
		case 'fcgi':
			header('Status: ' . $response_code, true, $response_code);
			break;

		case 'server':
			header('HTTP/1.0 ' . $response_code . ' ' . get_response_code_string($response_code), true, $response_code);
			break;
		}
	}
}






/*
 * http://www.php.net/manual/en/function.image-type-to-extension.php#77354
 * -->
 * http://www.php.net/manual/en/function.image-type-to-extension.php#79688
 */
if (!function_exists('image_type_to_extension'))
{
	function image_type_to_extension($type, $dot = true)
	{
		$e = array(1 => 'gif', 'jpeg', 'png', 'swf', 'psd', 'bmp', 'tiff', 'tiff', 'jpc', 'jp2', 'jpf', 'jb2', 'swc', 'aiff', 'wbmp', 'xbm');

		// We are expecting an integer.
		$t = (int)$type;
		if (!$t)
		{
			trigger_error('invalid IMAGETYPE_XXX(' . $type . ') passed to image_type_to_extension()', E_USER_NOTICE);
			return null;
		}
		if (!isset($e[$t]))
		{
			trigger_error('unidentified IMAGETYPE_XXX(' . $type . ') passed to image_type_to_extension()', E_USER_NOTICE);
			return null;
		}

		return ($dot ? '.' : '') . $e[$t];
	}
}


if (!function_exists('image_type_to_mime_type'))
{
	function image_type_to_mime_type($type)
	{
		$m = array(1 => 'image/gif', 'image/jpeg', 'image/png',
			'application/x-shockwave-flash', 'image/psd', 'image/bmp',
			'image/tiff', 'image/tiff', 'application/octet-stream',
			'image/jp2', 'application/octet-stream', 'application/octet-stream',
			'application/x-shockwave-flash', 'image/iff', 'image/vnd.wap.wbmp', 'image/xbm');

		// We are expecting an integer.
		$t = (int)$type;
		if (!$t)
		{
			trigger_error('invalid IMAGETYPE_XXX(' . $type . ') passed to image_type_to_mime_type()', E_USER_NOTICE);
			return null;
		}
		if (!isset($m[$t]))
		{
			trigger_error('unidentified IMAGETYPE_XXX(' . $type . ') passed to image_type_to_mime_type()', E_USER_NOTICE);
			return null;
		}
		return $m[$t];
	}
}






/// <summary>
/// <para>Code according to info found here: http://mathforum.org/library/drmath/view/51886.html</para>
///
/// <para>
/// Date: 06/29/98 at 13:12:44</para>
/// <para>
/// From: Doctor Peterson</para>
/// <para>
/// Subject: Re: Decimal To Fraction Conversion</para>
///
/// <para>
/// The algorithm I am about to show you has an interesting history. I
/// recently had a discussion with a teacher in England who had a
/// challenging problem he had given his students, and wanted to know what
/// others would do to solve it. The problem was to find the fraction
/// whose decimal value he gave them, which is essentially identical to
/// your problem! I wasn't familiar with a standard way to do it, but
/// solved it by a vaguely remembered Diophantine method. Then, my
/// curiosity piqued, and I searched the Web for information on the
/// problem and didn't find it mentioned in terms of finding the fraction
/// for an actual decimal, but as a way to approximate an irrational by a
/// fraction, where the continued fraction method was used. </para>
///
/// <para>
/// I wrote to the teacher, and he responded with a method a student of
/// his had come up with, which uses what amounts to a binary search
/// technique. I recognized that this produced the same sequence of
/// approximations that continued fractions gave, and was able to
/// determine that it is really equivalent, and that it is known to some
/// mathematicians (or at least math historians). </para>
///
/// <para>
/// After your request made me realize that this other method would be
/// easier to program, I thought of an addition to make it more efficient,
/// which to my knowledge is entirely new. So we're either on the cutting
/// edge of computer technology or reinventing the wheel, I'm not sure
/// which!</para>
///
/// <para>
/// Here's the method, with a partial explanation for how it works:</para>
///
/// <para>
/// We want to approximate a value m (given as a decimal) between 0 and 1,
/// by a fraction Y/X. Think of fractions as vectors (denominator,
/// numerator), so that the slope of the vector is the value of the
/// fraction. We are then looking for a lattice vector (X, Y) whose slope
/// is as close as possible to m. This picture illustrates the goal, and
/// shows that, given two vectors A and B on opposite sides of the desired
/// slope, their sum A + B = C is a new vector whose slope is between the
/// two, allowing us to narrow our search:</para>
///
/// <code>
/// num
/// ^
/// |
/// +  +  +  +  +  +  +  +  +  +  +
/// |
/// +  +  +  +  +  +  +  +  +  +  +
/// |                                  slope m=0.7
/// +  +  +  +  +  +  +  +  +  +  +   /
/// |                               /
/// +  +  +  +  +  +  +  +  +  +  D &lt;--- solution
/// |                           /
/// +  +  +  +  +  +  +  +  + /+  +
/// |                       /
/// +  +  +  +  +  +  +  C/ +  +  +
/// |                   /
/// +  +  +  +  +  + /+  +  +  +  +
/// |              /
/// +  +  +  +  B/ +  +  +  +  +  +
/// |          /
/// +  +  + /A  +  +  +  +  +  +  +
/// |     /
/// +  +/ +  +  +  +  +  +  +  +  +
/// | /
/// +--+--+--+--+--+--+--+--+--+--+--&gt; denom
/// </code>
///
/// <para>
/// Here we start knowing the goal is between A = (3,2) and B = (4,3), and
/// formed a new vector C = A + B. We test the slope of C and find that
/// the desired slope m is between A and C, so we continue the search
/// between A and C. We add A and C to get a new vector D = A + 2*B, which
/// in this case is exactly right and gives us the answer.</para>
///
/// <para>
/// Given the vectors A and B, with slope(A) &lt; m &lt; slope(B),
/// we can find consecutive integers M and N such that
/// slope(A + M*B) &lt; x &lt; slope(A + N*B) in this way:</para>
///
/// <para>
/// If A = (b, a) and B = (d, c), with a/b &lt; m &lt; c/d, solve</para>
///
/// <code>
///     a + x*c
///     ------- = m
///     b + x*d
/// </code>
///
/// <para>
/// to give</para>
///
/// <code>
///         b*m - a
///     x = -------
///         c - d*m
/// </code>
///
/// <para>
/// If this is an integer (or close enough to an integer to consider it
/// so), then A + x*B is our answer. Otherwise, we round it down and up to
/// get integer multipliers M and N respectively, from which new lower and
/// upper bounds A' = A + M*B and B' = A + N*B can be obtained. Repeat the
/// process until the slopes of the two vectors are close enough for the
/// desired accuracy. The process can be started with vectors (0,1), with
/// slope 0, and (1,1), with slope 1. Surprisingly, this process produces
/// exactly what continued fractions produce, and therefore it will
/// terminate at the desired fraction (in lowest terms, as far as I can
/// tell) if there is one, or when it is correct within the accuracy of
/// the original data.</para>
///
/// <para>
/// For example, for the slope 0.7 shown in the picture above, we get
/// these approximations:</para>
///
/// <para>
/// Step 1: A = 0/1, B = 1/1 (a = 0, b = 1, c = 1, d = 1)</para>
///
/// <code>
///         1 * 0.7 - 0   0.7
///     x = ----------- = --- = 2.3333
///         1 - 1 * 0.7   0.3
///
///     M = 2: lower bound A' = (0 + 2*1) / (1 + 2*1) = 2 / 3
///     N = 3: upper bound B' = (0 + 3*1) / (1 + 3*1) = 3 / 4
/// </code>
///
/// <para>
/// Step 2: A = 2/3, B = 3/4 (a = 2, b = 3, c = 3, d = 4)</para>
///
/// <code>
///         3 * 0.7 - 2   0.1
///     x = ----------- = --- = 0.5
///         3 - 4 * 0.7   0.2
///
///     M = 0: lower bound A' = (2 + 0*3) / (3 + 0*4) = 2 / 3
///     N = 1: upper bound B' = (2 + 1*3) / (3 + 1*4) = 5 / 7
/// </code>
///
/// <para>
/// Step 3: A = 2/3, B = 5/7 (a = 2, b = 3, c = 5, d = 7)</para>
///
/// <code>
///         3 * 0.7 - 2   0.1
///     x = ----------- = --- = 1
///         5 - 7 * 0.7   0.1
///
///     N = 1: exact value A' = B' = (2 + 1*5) / (3 + 1*7) = 7 / 10
/// </code>
///
/// <para>
/// which of course is obviously right.</para>
///
/// <para>
/// In most cases you will never get an exact integer, because of rounding
/// errors, but can stop when one of the two fractions is equal to the
/// goal to the given accuracy.</para>
///
/// <para>
/// [...]Just to keep you up to date, I tried out my newly invented algorithm
/// and realized it lacked one or two things. Specifically, to make it
/// work right, you have to alternate directions, first adding A + N*B and
/// then N*A + B. I tested my program for all fractions with up to three
/// digits in numerator and denominator, then started playing with the
/// problem that affects you, namely how to handle imprecision in the
/// input. I haven't yet worked out the best way to allow for error, but
/// here is my C++ function (a member function in a Fraction class
/// implemented as { short num; short denom; } ) in case you need to go to
/// this algorithm.
/// </para>
///
/// <para>[Edit [i_a]: tested a few stop criteria and precision settings;
/// found that you can easily allow the algorithm to use the full integer
/// value span: worst case iteration count was 21 - for very large prime
/// numbers in the denominator and a precision set at double.Epsilon.
/// Part of the code was stripped, then reinvented as I was working on a
/// proof for this system. For one, the reason to 'flip' the A/B treatment
/// (i.e. the 'i&1' odd/even branch) is this: the factor N, which will
/// be applied to the vector addition A + N*B is (1) an integer number to
/// ensure the resulting vector (i.e. fraction) is rational, and (2) is
/// determined by calculating the difference in direction between A and B.
/// When the target vector direction is very close to A, the difference
/// in *direction* (sort of an 'angle') is tiny, resulting in a tiny N
/// value. Because the value is rounded down, A will not change. B will,
/// but the number of iterations necessary to arrive at the final result
/// increase significantly when the 'odd/even' processing is not included.
/// Basically, odd/even processing ensures that once every second iteration
/// there will be a major change in direction for any target vector M.]
/// </para>
///
/// <para>[Edit [i_a]: further testing finds the empirical maximum
/// precision to be ~ 1.0E-13, IFF you use the new high/low precision
/// checks (simpler, faster) in the code (old checks have been commented out).
/// Higher precision values cause the code to produce very huge fractions
/// which clearly show the effect of limited floating point accuracy.
/// Nevetheless, this is an impressive result.
///
/// I also changed the loop: no more odd/even processing but now we're
/// looking for the biggest effect (i.e. change in direction) during EVERY
/// iteration: see the new x1:x2 comparison in the code below.
/// This will lead to a further reduction in the maximum number of iterations
/// but I haven't checked that number now. Should be less than 21,
/// I hope. ;-) ]
/// </para>
/// </summary>
class Fraction
{
	public $num;        // integer
	public $denom;      // integer

	public function __construct($n, $d = null)
	{
		if ($d !== null)
		{
			// direct specified fraction: num / denom
			$this->num = (integer)round($n);
			$this->denom = (integer)round($d);
		}
		else
		{
			$f = self::toFract((double)$n, 1.0E-13);  // float.Epsilon
			$this->num = $f->num;
			$this->denom = $f->denom;
		}
	}

	public static function toFract(double $val, double $Precision)
	{
		// find nearest fraction
		$intPart = (integer)$val;
		$val -= $intPart;

		// improvement for start: don't start with the angle = range [0/1 .. 1/1] but instead start with finer-grained approx:
		// guess.num = int(val * (2 * 3 * 5 * 7 * 11))  // all the primes from 2 onwards or you won't get certain fractions when using limited precision
		// guess.denom = 2 * 3 * 5 * 7 * 11;
		// and that's the lower bound. Upper bound = guess.num + 1, same .denom
		// Of course, then you'll need the gcd() call at the end before returning the fraction to the caller.

		$low_num = 0;
		$low_denom = 1;           // "A" = 0/1 (a/b)
		$high_num = 1;
		$high_denom = 1;          // "B" = 1/1 (c/d)

		$low_num = (integer)($val * 2310);
		$low_denom = 2310;        // 2 * 3 * 5 * 7 * 11
		$high_num = $low_num + 1;
		$high_denom = 2310;

		for (;;)
		{
			//assert(low.Val <= val);
			//assert(high.Val >= val);

			//         b*m - a
			//     x = -------
			//         c - d*m
			/* double */ $testLow = $low_denom * $val - $low_num;
			/* double */ $testHigh = $high_num - $high_denom * $val;
			// test for match:
			//
			// m - a/b < precision
			//
			// ==>
			//
			// b * m - a < b * precision
			//
			// which is happening here: check both the current A and B fractions.
			//if (testHigh < high.denom * Precision)
			if ($testHigh < $Precision) // [i_a] speed improvement; this is even better for irrational 'val'
			{
				break; // high is answer
			}
			//if (testLow < low.denom * Precision)
			if ($testLow < $Precision) // [i_a] speed improvement; this is even better for irrational 'val'
			{
				// low is answer
				$high_num = $low_num;
				$high_denom = $low_denom;
				break;
			}

			$x1 = $testHigh / $testLow;
			$x2 = $testLow / $testHigh;

			// always choose the path where we find the largest change in direction:
			if ($x1 > $x2)
			{
				//double x1 = testHigh / testLow;
				// safety checks: are we going to be out of integer bounds?
				if (($x1 + 1) * $low_denom + $high_denom >= (double)PHP_INT_MAX)
				{
					break;
				}

				/* integer */ $n = (integer)$x1;    // lower bound for m
				//int m = n + 1;    // upper bound for m

				//     a + x*c
				//     ------- = m
				//     b + x*d
				/* integer */ $h_num = $n * $low_num + $high_num;
				/* integer */ $h_denom = $n * $low_denom + $high_denom;

				//integer l_num = m * low.num + high.num;
				//integer l_denom = m * low.denom + high.denom;
				$l_num = $h_num + $low_num;
				$l_denom = $h_denom + $low_denom;

				$low_num = $l_num;
				$low_denom = $l_denom;
				$high_num = $h_num;
				$high_denom = $h_denom;
			}
			else
			{
				//double x2 = testLow / testHigh;
				// safety checks: are we going to be out of integer bounds?
				if ($low_denom + ($x2 + 1) * $high_denom >= (double)PHP_INT_MAX)
				{
					break;
				}

				/* integer */ $n = (integer)$x2;    // lower bound for m
				//integer m = n + 1;    // upper bound for m

				//     a + x*c
				//     ------- = m
				//     b + x*d
				/* integer */ $l_num = $low_num + $n * $high_num;
				/* integer */ $l_denom = $low_denom + $n * $high_denom;

				//integer h_num = low.num + m * high.num;
				//integer h_denom = low.denom + m * high.denom;
				$h_num = $l_num + $high_num;
				$h_denom = $l_denom + $high_denom;

				$high_num = $h_num;
				$high_denom = $h_denom;
				$low_num = $l_num;
				$low_denom = $l_denom;
			}
			//assert(low.Val <= val);
			//assert(high.Val >= val);
		}

		$high_num += $high_denom * $intPart;
		return new Fraction($high_num, $high_denom);
	}

	public static function Test()
	{
		//Fraction ret;
		//double vut;

		$vut = 0.1;
		$ret = Fraction::toFract($vut);
		assert(abs($vut - $ret->Val()) < 1E-9);
		$vut = 0.99999997;
		$ret = Fraction::toFract($vut);
		assert(abs($vut - $ret->Val()) < 1E-9);
		$vut = (0x40000000 - 1.0) / (0x40000000 + 1.0);
		$ret = Fraction::toFract($vut);
		assert(abs($vut - $ret->Val()) < 1E-9);
		$vut = 1.0 / 3.0;
		$ret = Fraction::toFract($vut);
		assert(abs($vut - $ret->Val()) < 1E-9);
		$vut = 1.0 / (0x40000000 - 1.0);
		$ret = Fraction::toFract($vut);
		assert(abs($vut - $ret->Val()) < 1E-9);
		$vut = 320.0 / 240.0;
		$ret = Fraction::toFract($vut);
		assert(abs($vut - $ret->Val()) < 1E-9);
		$vut = 6.0 / 7.0;
		$ret = Fraction::toFract($vut);
		assert(abs($vut - $ret->Val()) < 1E-9);
		$vut = 320.0 / 241.0;
		$ret = Fraction::toFract($vut);
		assert(abs($vut - $ret->Val()) < 1E-9);
		$vut = 720.0 / 577.0;
		$ret = Fraction::toFract(vut);
		assert(abs($vut - $ret->Val()) < 1E-9);
		$vut = 2971.0 / 3511.0;
		$ret = Fraction::toFract(vut);
		assert(abs($vut - $ret->Val()) < 1E-9);
		$vut = 3041.0 / 7639.0;
		$ret = Fraction::toFract(vut);
		assert(abs($vut - $ret->Val()) < 1E-9);
		$vut = 1.0 / Math.Sqrt(2);
		$ret = Fraction::toFract(vut);
		assert(abs($vut - $ret->Val()) < 1E-9);
		$vut = Math.PI;
		$ret = Fraction::toFract(vut);
		assert(abs($vut - $ret->Val()) < 1E-9);
	}

	public /* double */ function Val()
	{
		return ((double)$this->num) / $this->denom;
	}
}

class RatioUtils
{
	/// <summary>
	/// Puts x and y in simplest form, by dividing by all their factors.
	/// </summary>
	/// <param name="x">First number to reduce</param>
	/// <param name="y">Second number to reduce</param>
	public static function reduce(/* ref integer */ &$x, /* ref integer */ &$y)
	{
		$g = self::gcd($x, $y);
		$x /= $g;
		$y /= $g;
	}

	public static /* integer */ function gcd(/* integer */ $x, /* integer */ $y)
	{
		while ($y != 0)
		{
			$t = $y;
			$y = $x % $y;
			$x = $t;
		}
		return $x;
	}

	public static function approximate($val, /* out integer */ &$x, /* out integer */ &$y, /* integer */ $limit = 5000)
	{
		// Fraction.Test();
		$f = Fraction::toFract((double)$val);

		$x = $f->num;
		$y = $f->denom;

		reduce($x, $y);
		// [i_a] ^^^ initial tests with the new algo show this is
		// rather unnecessary, but we'll keep it anyway, just in case.
		//
		// [Edit] needed now we have the faster start approx in there at denom = 2 * 3 * 5 * 7 * 11 = 2310
	}
}

