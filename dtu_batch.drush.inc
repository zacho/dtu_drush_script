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

  $track_directories = array();
  $track_directories = scandir($input_directory);

  //loop through the track directories, the acutal files are in subdirs
  foreach ($track_directories as $track_directory)
  {
    if ($track_directory === '.' || $track_directory === '..' || !is_dir($input_directory.'/'.$track_directory)) {
      continue;
    }

    $track_id = $track_directory;
    $track_name_file = $input_directory . '/' . $track_directory . "/track.label.txt";
    if (is_file($track_name_file)) {
      $track_name = trim(file_get_contents($track_name_file));
    } 
    else {
      drush_log("No track name for track directory " . $track_id);
      drupal_set_message("No track name for track directory " . $track_id);
      continue;
    }

    $session_directories = array();
    $session_directories = scandir($input_directory.'/'.$track_directory);

    //loop through session directories
    foreach ($session_directories as $session_directory)
    {
      if ($session_directory === '.' || $session_directory === '..' || !is_dir($input_directory.'/'.$track_directory.'/'.$session_directory)) {
        continue;
      }

      $session_id = $session_directory;
      $session_name_file = $input_directory . '/' . $track_directory . '/' . $session_directory . "/session.label.txt";
      if (is_file($session_name_file)) {
        $session_name = trim(file_get_contents($session_name_file));
      } 
      else {
        drush_log("No session name for session directory " . $session_id);
        drupal_set_message("No session name for session directory " . $session_id);
        continue;
      }

      $talk_directories = array();
      $talk_directories = scandir($input_directory.'/'.$track_directory.'/'.$session_directory);

      //loop through talk directories
      foreach ($talk_directories as $talk_directory)
      {

        $mxd_files = NULL;

        if ($talk_directory === '.' || $talk_directory === '..') {
          continue;
        }

        $mxd_file = false;
        $paper_file = false;
        $slide_file = false;
        $sdvideo_file = false;
        $hdvideo_file = false;

        //this dir path is a sub dir of $input_directory
        $directory_path = $input_directory.'/'.$track_directory.'/'.$session_directory.'/'.$talk_directory;

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
          $properties = array(
	    'track' => array($track_id => $track_name),
	    'session' => array($session_id => $session_name),
          );

//          drupal_set_message(sprintf("Track: %s\nSession: %s\nMXD: %s\nPaper: %s\nSlide: %s\nVideo: %s\nCollection: %s\nRelationship: %s\n\n",
//                                     $track_name,$session_name,$mxd_file,$paper_file,$slide_file,$video_file,$collection_pid,$relationship));
//          continue;

          cdio2011_create_object($mxd_file,$paper_file,$slide_file,$video_file,$collection_pid,$relationship,$properties);
          sleep(3);

        }

      }

    }
  }
}

/**
 * This function ingests per an object
 */
function cdio2011_create_object($mxd_file,$paper_file,$slide_file,$video_file,$collection_pid,$relationship,$properties) {
  global $base_url;
  drush_log('Ingesting: ' . $mxd_file);
  drupal_set_message('Ingesting: ' . $mxd_file);
  module_load_include('inc', 'fedora_repository', 'api/fedora_item');
  module_load_include('inc', 'fedora_repository', 'MimeClass');
  $child_relationship = "isDerivationOf";
  $mime_class = new MimeClass();

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

  drush_log('using label ' . $label);
  drupal_set_message('using label ' . $label);
  $mxd_xml = file_get_contents($mxd_file);

  list($track_id,$track_name) = each($properties['track']);
  list($session_id,$session_name) = each($properties['session']);

  $track_pid = sprintf("cdio2011:track_%s",$track_id);
  $session_pid = sprintf("cdio2011:session_%s",$session_id);

  $track_and_session_xml = sprintf('<dtu:conference xmlns:dtu="http://discoverygarden.cvt.dk/conference_level">
  <dtu:track code="%s" label="%s" pid="%s">
  <dtu:session code="%s" label="%s" pid="%s"></dtu:session>
  </dtu:track>
  </dtu:conference>',
  $track_id,$track_name,$track_pid,$session_id,$session_name,$session_pid);

  $folder_tn = $base_url."/sites/all/modules/islandora_dtu_mxd_talk/images/Crystal_Clear_filesystem_folder_grey.png";
  $collection_policy = $base_url."/sites/all/modules/islandora_dtu_mxd_talk/xml/COLLECTION_POLICY.xml";

//return;
//  print "object $mxd_file has track $track_id = $track_name and session $session_id = $session_name\n";

  $track_exists = @file_get_contents(variable_get('fedora_base_url','http://localhost:8080/fedora')."/objects/".$track_pid."?format=xml");
  if (!$track_exists) {
    // then it doesn't yet exist
    drupal_set_message("PID $track_pid does not yet exist in the Repository","warning");
    drush_log("PID $track_pid does not yet exist in the Repository");
    $track_item = Fedora_Item::ingest_new_item($track_pid,'A',$track_name); //new skeleton object
    $track_item->add_relationship($relationship, $collection_pid);
    $track_item->add_relationship('hasModel', 'islandora:collectionCModel', FEDORA_MODEL_URI);
    $track_item->add_datastream_from_url($folder_tn,'TN','Thumbnail.png','image/png','M','added jpeg thumbnail stream');
    $track_item->add_datastream_from_url($collection_policy,'COLLECTION_POLICY','Collection Policy','text/xml','X','added collection policy');
    sleep(1);
  }

  $track_item = new Fedora_Item($track_pid);

  $session_exists = @file_get_contents(variable_get('fedora_base_url','http://localhost:8080/fedora')."/objects/".$session_pid."?format=xml");
  if (!$session_exists) {
    // then it doesn't yet exist
    drupal_set_message("PID $session_pid does not yet exist in the Repository","warning");
    drush_log("PID $session_pid does not yet exist in the Repository");
    $session_item = Fedora_Item::ingest_new_item($session_pid,'A',$session_name); //new skeleton object
    $session_item->add_relationship($relationship, $track_pid);
    $session_item->add_relationship('hasModel', 'islandora:collectionCModel', FEDORA_MODEL_URI);
    $session_item->add_datastream_from_url($folder_tn,'TN','Thumbnail.png','image/png','M','added jpeg thumbnail stream');
    $session_item->add_datastream_from_url($collection_policy,'COLLECTION_POLICY','Collection Policy','text/xml','X','added collection policy');
    sleep(1);
  }

  $session_item = new Fedora_Item($session_pid);

//  var_dump($track_item->get_relationships());
//  var_dump($session_item->get_relationships());

//var_dump($mxd_xml);

//return;

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
  
  $object->add_datastream_from_string($track_name, 'TRACK', 'Track', 'text/plain', 'M', 'added track name');
  $object->add_datastream_from_string($session_name, 'SESSION', 'Session', 'text/plain', 'M', 'added session name');
  $object->add_datastream_from_string($track_and_session_xml, 'TRACK_AND_SESSION', 'Track and Session XML', 'text/xml', 'X', 'added session name');

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
      $default_tn_url = $base_url."/sites/all/modules/islandora_dtu_mxd_talk/images/Crystal_Clear_mimetype_document.png";
    } elseif( $video_file ) {
      $default_tn_url = $base_url."/sites/all/modules/islandora_dtu_mxd_talk/images/video_120.png";
    } else {
      $default_tn_url = $base_url."/sites/all/modules/islandora_dtu_mxd_talk/images/mime.png";
    }
    $object->add_datastream_from_url($default_tn_url,'TN','Thumbnail.png','image/png','M','added jpeg thumbnail stream');
  }
  $object->add_relationship($relationship, $session_pid);
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
      $video_object->add_datastream_from_url($base_url.'/sites/all/modules/islandora_dtu_mxd_talk/images/video.png','TN','Thumbnail.png','image/png','M','Added Thumbnail');
    } else {
      //ingest from file
      $video_mimetype = $mime_class->get_mimetype($video_file);
      $video_stream_label = "Video.".$video_file_info['extension'];
      $video_object->add_datastream_from_file($video_file,'OBJ',$video_stream_label,$video_mimetype,'M','Added Video content');
      $video_object->add_datastream_from_url($base_url.'/sites/all/modules/islandora_dtu_mxd_talk/images/video.png','TN','Thumbnail.png','image/png','M','Added Thumbnail');
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
