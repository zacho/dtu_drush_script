<?php

/**
 * This script needs to be updated by namespacing the functions
 * as is there could be function name conflicts with other drush scripts
 * @todo: add logging before a full ingest is done
 */

/**
 * drush hook
 */
function drush_dtu_batch_cdio2011_batch($collection_pid, $relationship, $input_directory) {
  drush_log('Checking input.');
  module_load_include('inc', 'fedora_repository', 'CollectionClass');
  set_time_limit(0); //this could be long so lets have infinite time on our hands

  drush_log('Current working directory ' . getcwd());
  if (isset($collection_pid)) {
    drush_log("Used collection pid of " . $collection_pid);
  }
  else {
    drush_log("No collection pid specified");
    return;
  }
  if (isset($relationship)) {
    drush_log("and relationship of " . $relationship);
  }
  else {
    drush_log("No relationship specified");
    return;
  }
  if (!isset($input_directory)) {
    drush_log("No intput path specified.");
    return;
  }

  cdio2011_doAction($collection_pid, $relationship, $input_directory);
}

/**
 * ingest cdio2011 objects
 * @param <type> $pid
 * @param <type> $relationship
 * @param <type> $input_path
 */
function cdio2011_doAction($collection_pid, $relationship, $input_directory) {

  drush_log('Starting ingest');
  drush_log("Data path: " . $input_directory);

  $directories = array();
  $directories = scandir($input_directory);

  //loop through the mxd directories, the acutal files are in subdirs
  foreach ($directories as $directory) {
    $mxd_files = NULL;

    if ($directory === '.' || $directory === '..') {
      continue;
    }

    $mxd_file = false;
    $paper_file = false;
    $slide_file = false;
    $sdvideo_file = false;
    $hdvideo_file = false;

    //this dir path is a sub dir of $input_directory
    $directory_path = $input_directory . '/' . $directory;

    //traverse the sub directories that have mxd in them.
    if (is_dir($directory_path)) {
      drush_log('Searching for MXD in directory: ' . $directory);
      $files = scandir($directory_path);

      if( !in_array ( 'mxd.xml', $files ) ) {
        drush_log('No MXD in this directory, skipping...' );
        continue;
      }
      
      $mxd_file = $directory_path . '/mxd.xml';
      $paper_file = in_array( 'paper.pdf', $files ) ? $directory_path.'/'.'paper.pdf' : false;
      $slide_file = in_array( 'slide.pdf', $files ) ? $directory_path.'/'.'slide.pdf' : false;
      $video_file = in_array( 'video.360.mp4', $files ) ? $directory_path.'/'.'video.360.mp4' : false;
      if (!$video_file)
        $video_file = in_array( 'video.720.mp4', $files ) ? $directory_path.'/'.'video.720.mp4' : false;
      if (!$video_file)
        $video_file = in_array( 'video.360.url', $files ) ? $directory_path.'/'.'video.360.url' : false;
      if (!$video_file)
        $video_file = in_array( 'video.720.url', $files ) ? $directory_path.'/'.'video.720.url' : false;

      drush_log('mxd file name:' .$mxd_file);

      cdio2011_create_object($mxd_file,$paper_file,$slide_file,$video_file,$collection_pid,$relationship);
      sleep(4);

    }
  }
}

/**
 * This function ingests per an object
 */
function cdio2011_create_object($mxd_file,$paper_file,$slide_file,$video_file, $collection_pid, $relationship) {
  drush_log('Ingesting: ' . $mxd_file);
  drupal_set_message('Ingesting: ' . $mxd_file);
  module_load_include('inc', 'fedora_repository', 'api/fedora_item');
  module_load_include('inc', 'fedora_repository', 'MimeClass');
  $child_relationship = "isMemberOf";
  $mime_class = new MimeClass();

#  $xml = simplexml_load_string(file_get_contents($mxd_file));
#  $xml->registerXPathNamespace('mxd', "http://mx.forskningsdatabasen.dk/ns/mxd/1.2");
  $xsl = new DOMDocument();
  $xsl->load("/var/www/sites/all/modules/islandora_content_model_forms/transforms/mxd_to_dc.xsl");
  $proc = new XSLTProcessor();
  $proc->importStyleSheet($xsl);
  $xml = new DomDocument();
  $xml->load($mxd_file);
  $dc = $proc->transformToDoc($xml->documentElement);
  $xpath = new DOMXPath($dc);
  // Set the Label
  $results = $xpath->query("*[local-name()='title']");
  $label = utf8_decode($results->item(0)->nodeValue);

#  $label = $xml->xpath('//mxd:title/mxd:original/mxd:main');
#  $label = (string) $label[0]; //this mxd only has the one title
  drush_log('using label ' . $label);
  drupal_set_message('using label ' . $label);
  $mxd_xml = file_get_contents($mxd_file);
#var_dump($dc_xml);

#return;

  $output = array();
  //uses fedora settings taken for islandora drupal settings
  $new_pid = Fedora_Item::get_next_PID_in_namespace('cdio2011'); //hard coding pid namespace could be passed as a variable if needed
  drush_log("creating object with pid " . $new_pid);
  $object = Fedora_Item::ingest_new_item($new_pid, 'A', $label); //create a new skeleton object
  drush_log("created object $new_pid now adding datastreams");

  // Set the Pid
  $results = $xpath->query("*[local-name()='identifier']");
  $results->item(0)->nodeValue = $new_pid;

  $dc_xml = $dc->saveXML();

  if ($object->modify_datastream_by_value($dc_xml, 'DC', 'Dublin Core Record for this object', 'text/xml', 'X', 'updated DC stream')) {
    drush_log('updated DC stream to ' . $new_pid);
  }
  else {
    drush_log('Failed updating DC stream to ' . $new_pid);
    exit;
  }

  if ($object->add_datastream_from_string($mxd_xml, 'MXD', 'DDF-MXD Metadata.xml', 'text/xml', 'X', 'added MXD Stream')) {
    drush_log('added MXD stream to ' . $new_pid);
  }
  else {
    drush_log('Failed adding MXD stream to ' . $new_pid . " using url $mxd_file");
    exit;
  }

  //primary conversions:
  if( $paper_file ) {
    $paper_file_tn = $paper_file."_TN.jpg";
    exec("/usr/bin/convert \"$paper_file\"\[0\] -colorspace RGB -thumbnail 120x128 \"$paper_file_tn\"", $output, $returnValue);
    if($returnValue == '0'){
      $object->add_datastream_from_file($paper_file_tn,'TN','Thumbnail.jpg','image/jpeg','M','added jpeg thumbnail stream');
      file_delete($paper_file_tn);
    } else {
      drush_log('failed to add TN to '.$new_pid);
    }
    $paper_file_swf = $paper_file."_SWF.swf";
    exec("pdf2swf \"$paper_file\" -o \"$paper_file_swf\" -T 9 -f", $output, $returnValue);
    if($returnValue == '0'){
      $object->add_datastream_from_file($paper_file_swf,'OBJSWF','Paper.swf','application/x-shockwave-flash','M','added SWF paper stream');
      file_delete($paper_file_swf);
    } else {
      drush_log('failed to add SWF to '.$new_pid);
    }
    if ($object->add_datastream_from_file($paper_file, 'OBJ', 'Paper.pdf', 'application/pdf', 'M', 'Added PDF Paper stream')) {
      drush_log('added PDF stream to ' . $new_pid);
    }
    else {
      drush_log('Failed adding PDF stream to ' . $new_pid);
    }
  } else {
    // no paper means we need to choose a default TN:
    if( $slide_file ) {
      $default_tn_url = "http://localhost/sites/all/modules/islandora_dtu_mxd_talk/images/Crystal_Clear_mimetype_document.png";
    } elseif( $video_file ) {
      $default_tn_url = "http://localhost/sites/all/modules/islandora_dtu_mxd_talk/images/video_120.png";
    } else {
      $default_tn_url = "http://localhost/sites/all/modules/islandora_dtu_mxd_talk/images/mime.png";
    }
    $object->add_datastream_from_url($default_tn_url,'TN','Thumbnail.png','image/png','M','added jpeg thumbnail stream');
  }
  $object->add_relationship($relationship, $collection_pid);
  $object->add_relationship('hasModel', 'islandora:dtu_mxd_talk_cmodel', FEDORA_MODEL_URI);


  // child objects - slides
  if( $slide_file ) {
    $slide_pid = $new_pid . "_slide_" . base_convert(mt_rand(0x19A100, 0x39AA3FF), 10, 36);
    $slide_label = $label . "(Slides)";
    $slide_object = Fedora_Item::ingest_new_item($slide_pid, 'A', $slide_label); //create a new skeleton object
    drush_log("created object $slide_pid now adding datastreams");

    $slide_file_tn = $slide_file."_TN.jpg";
    exec("/usr/bin/convert \"$slide_file\"\[0\] -colorspace RGB -thumbnail 120x128 \"$slide_file_tn\"", $output, $returnValue);
    if($returnValue == '0'){
      $slide_object->add_datastream_from_file($slide_file_tn,'TN','Thumbnail.jpg','image/jpeg','M','added jpeg thumbnail stream');
      file_delete($slide_file_tn);
    } else {
      drush_log('failed to add TN to '.$new_pid);
    }
    $slide_file_swf = $slide_file."_SWF.swf";
    exec("pdf2swf \"$slide_file\" -o \"$slide_file_swf\" -T 9 -f", $output, $returnValue);
    if($returnValue == '0'){
      $slide_object->add_datastream_from_file($slide_file_swf,'OBJSWF','Slides.swf','image/jpeg','M','added jpeg thumbnail stream');
      file_delete($slide_file_swf);
    }
    if ($slide_object->add_datastream_from_file($slide_file, 'OBJ', 'Slides.pdf', 'application/pdf', 'M', 'Added PDF Slide stream')) {
      drush_log('added PDF stream to ' . $slide_pid);
    }
    else {
      drush_log('Failed adding PDF stream to ' . $slide_pid);
    }
    $slide_object->add_relationship($child_relationship, $new_pid);
    $slide_object->add_relationship('hasModel', 'islandora:dtu_dc_slide_cmodel', FEDORA_MODEL_URI);
  }

  // child objects - video
  if( $video_file ) {
    $video_pid = $new_pid . "_video_" . base_convert(mt_rand(0x19A100, 0x39AA3FF), 10, 36);
    $video_label = $label . "(Video)";
    $video_object = Fedora_Item::ingest_new_item($video_pid, 'A', $video_label); //create a new skeleton object
    drush_log("created object $video_pid now adding datastreams");

    $video_file_info = pathinfo($video_file);
    if( $video_file_info['extension'] == 'url' ) {
      drupal_set_message("Fetching video from ".$video_file.". This may take some time.");
      drush_log("Fetching video from ".$video_file.". This may take some time.");
      //ingest from url
      // get the URL
      $video_url = trim(file_get_contents($video_file));
      // get the mimetype
      $video_mimetype = $mime_class->get_mimetype($video_url);
      $video_url_info = pathinfo($video_url);
      $video_stream_label = "Video.".$video_url_info['extension'];
      $video_object->add_datastream_from_url($video_url,'OBJ',$video_stream_label,$video_mimetype,'M','Added Video content');
      $video_object->add_datastream_from_url($video_url,'REF',$video_stream_label,$video_mimetype,'R','Added External Video Reference stream');
      $video_object->add_datastream_from_url('http://localhost/sites/all/modules/islandora_dtu_mxd_talk/images/video.png','TN','Thumbnail.png','image/png','M','Added Thumbnail');
    } else {
      //ingest from file
      $video_mimetype = $mime_class->get_mimetype($video_file);
      $video_stream_label = "Video.".$video_file_info['extension'];
      $video_object->add_datastream_from_file($video_file,'OBJ',$video_stream_label,$video_mimetype,'M','Added Video content');
      $video_object->add_datastream_from_url('http://localhost/sites/all/modules/islandora_dtu_mxd_talk/images/video.png','TN','Thumbnail.png','image/png','M','Added Thumbnail');
    }
    $video_object->add_relationship($child_relationship, $new_pid);
    $video_object->add_relationship('hasModel', 'islandora:dtu_dc_video_cmodel', FEDORA_MODEL_URI);
  }

}

/**
 * A helper function that checks if the string ends with the test.
 * @param string $string
 * @param string $test
 * @return boolean
 */
function endswith($string, $test) {
  $strlen = strlen($string);
  $testlen = strlen($test);
  if ($testlen > $strlen)
    return false;
  return substr_compare($string, $test, -$testlen) === 0;
}

function dtu_batch_drush_command() {
  $items = array();

  $items['cdio2011_batch'] = array(
    'description' => "gets a list of files from a web accessible location and ingest them into Fedora.  This is currently hard coded to look for files that end with .tif and for each tif will log for a .xml and a .jpg and add these as datastreams",
    'arguments' => array(
      'collection_pid' => 'The pid of the collection object, this is the collection the new objects will be related to',
      'relationship' => 'The relationship the objects have to the collection object.  For example isMemberOf is a common relationship.',
      'input_directory' => 'The path to the mxd files directories parent (absolute file system path, /var/www/http/drupal/sites/default/files).',
    ),
    'examples' => array(
      'drush -u 1 --uri=http://dgitest.conferences.dtu.dk/ cdio2011_batch dtu:cdio2011 isMemberOfCollection /home/denmark/hierarchical',
    ),
    'aliases' => array('cdio2011batch'),
      'bootstrap' => DRUSH_BOOTSTRAP_DRUPAL_LOGIN, // we can pass in users id on the command line using drush -u.
    );

  return $items;
}
?>
