#include <asf_contact.h>
#include <asf_license.h>
#include "geotiff_flavors.h"
#include "asf_import.h"
#include "asf_meta.h"
#include "asf_nan.h"
#include "ceos.h"
#include "decoder.h"
#include "find_geotiff_name.h"
#include "find_arcgis_geotiff_aux_name.h"
#include "import_arcgis_geotiff.h"
#include "get_ceos_names.h"
#include "get_stf_names.h"
#include "asf_raster.h"
#include <ctype.h>

/*
These next few functions are used to fix scaling errors in the data that
result from the PP using an incorrect swath velocity during some of
the calculations.

Here is a summary of the fixes as described by Orion Lawlor in an e-mail
on 3/14/06:

xpix_ypix prints out these values for an ARDoP image.
> > azimuth pixel size at scene center: 3.9648920 meters   (xpix_ypix)
> > ASF geolocate azimuth velocity: 6660.144 m/s   (xpix_ypix)
> > PP swath velocity: 6626.552 m/s = ...
The velocities should be about the same for the corresponding
L1 image.

The PP L1 pixel spacing is supposed to always be 12.5m.
But because the PP miscalulates the swath velocity, L1 images actually
have a pixel spacing of:
PP spacing * real velocity / PP velocity = real spacing
12.5 * 6660.144/6626.552 = 12.5633662876 m

An ARDOP image's pixel spacing is properly computed by xpix_ypix (and
not in the "yPix" field of the .meta file!) and multilooked, so the
ARDOP image pixel spacing is really:
3.9648920 m/pix * 5-pixel ARDOP multilook = 19.8244600 m/pix

The expected L1-to-multilooked-ARDOP image scale factor is just the
ratio of the two image's pixel spacings:
19.8244600 m/pix / 12.5633662876 m/pix = 1.5779576545
*/

void fix_ypix(const char *outBaseName, double correct_y_pixel_size)
{
    asfPrintStatus("Applying y pixel size correction to the metadata...\n");

    char outMetaName[256];
    sprintf(outMetaName, "%s%s", outBaseName, TOOLS_META_EXT);

    meta_parameters *omd = meta_read(outMetaName);

    asfPrintStatus("Original y pixel size: %g\n",
        omd->general->y_pixel_size);
    asfPrintStatus("            corrected: %g\n",
        correct_y_pixel_size);

    omd->general->y_pixel_size = correct_y_pixel_size;
    meta_write(omd, outMetaName);
    meta_free(omd);
}

float get_default_azimuth_scale(const char *outBaseName)
{
    char outMetaName[256];
    sprintf(outMetaName, "%s%s", outBaseName, TOOLS_META_EXT);

    meta_parameters *omd = meta_read(outMetaName);

    float pp_velocity, corrected_velocity;
    xpyp_getVelocities(omd, &pp_velocity, &corrected_velocity);

    asfPrintStatus("       PP Velocity: %g\n", pp_velocity);
    asfPrintStatus("Corrected Velocity: %g\n", corrected_velocity);

    float qua, az_pixsiz;
    xpyp_getPixSizes(omd, &qua, &az_pixsiz);

    asfPrintStatus("      y pixel size: %g (xpix_ypix)\n", az_pixsiz);
    asfPrintStatus("      y pixel size: %g (metadata)\n",
        omd->general->y_pixel_size);

    float real_spacing =
        omd->general->y_pixel_size * corrected_velocity / pp_velocity;

    float scale = az_pixsiz / real_spacing;

    asfPrintStatus("             Scale: %g\n\n", scale);
    meta_free(omd);

    return scale;
}

float get_default_ypix(const char *outBaseName)
{
    char outMetaName[256];
    sprintf(outMetaName, "%s%s", outBaseName, TOOLS_META_EXT);

    meta_parameters *omd = meta_read(outMetaName);

    float qua, az_pixsiz;
    xpyp_getPixSizes(omd, &qua, &az_pixsiz);
    meta_free(omd);

    return az_pixsiz;
}

/******************************************************************************
* Lets rock 'n roll!
*****************************************************************************/
// Convenience wrapper, without the kludgey "flags" array
int asf_import(radiometry_t radiometry, int db_flag,
               char *format_type, char *image_data_type, char *lutName, 
	       char *prcPath, double lowerLat, double upperLat, 
	       double *p_range_scale, double *p_azimuth_scale, 
	       double *p_correct_y_pixel_size, char *inMetaNameOption,
	       char *inBaseName, char *outBaseName)
{
    char **inBandName, inDataName[512]="", inMetaName[512]="";
    char unscaledBaseName[256]="", bandExt[10]="", tmp[10]="";
    int do_resample;
    int do_metadata_fix;
    double range_scale = -1, azimuth_scale = -1, correct_y_pixel_size = 0;
    int ii, kk, nBands;

    asfPrintStatus("Importing: %s\n", inBaseName);

    // Allocate memory
    inBandName = (char **) MALLOC(MAX_BANDS*sizeof(char *));
    for (ii=0; ii<MAX_BANDS; ii++)
      inBandName[ii] = (char *) MALLOC(512*sizeof(char));

    // Determine some flags
    do_resample = p_range_scale != NULL && p_azimuth_scale != NULL;
    do_metadata_fix = p_correct_y_pixel_size != NULL;

    if (p_range_scale)
        range_scale = *p_range_scale;
    if (p_azimuth_scale)
        azimuth_scale = *p_azimuth_scale;
    if (p_correct_y_pixel_size)
        correct_y_pixel_size = *p_correct_y_pixel_size;

    strcpy(unscaledBaseName, outBaseName);
    if (do_resample) {
        strcat(unscaledBaseName, "_unscaled");
    }

    /* Ingest all sorts of flavors of CEOS data */
    if (strncmp(format_type, "CEOS", 4) == 0) {
        asfPrintStatus("   Data format: %s\n", format_type);
        if (inMetaNameOption == NULL)
            require_ceos_pair(inBaseName, inBandName, inMetaName, &nBands);
        else {
            /* Allow the base name to be different for data & meta */
            require_ceos_data(inBaseName, inBandName, &nBands);
            require_ceos_metadata(inMetaNameOption, inMetaName);
        }
	for (ii=0; ii<nBands; ii++) {
	  strcpy(bandExt, "");
	  if (strncmp(inBandName[ii], "IMG-HH", 6)==0) 
	    strcpy(bandExt, "HH");
	  if (strncmp(inBandName[ii], "IMG-HV", 6)==0) 
	    strcpy(bandExt, "HV");
	  if (strncmp(inBandName[ii], "IMG-VH", 6)==0) 
	    strcpy(bandExt, "VH");
	  if (strncmp(inBandName[ii], "IMG-VV", 6)==0) 
	    strcpy(bandExt, "VV");
	  for (kk=1; kk<10; kk++) {
	    sprintf(tmp, "IMG-0%d", kk);
	    if (strncmp(inBandName[ii], tmp, 6)==0) 
	      sprintf(bandExt, "%d", kk);
	  }
	  import_ceos(inBandName[ii], bandExt, ii+1, nBands, inBaseName, lutName, 
		      unscaledBaseName, radiometry, db_flag);
	}
    }
    /* Ingest ENVI format data */
    else if (strncmp(format_type, "ENVI", 4) == 0) {
        asfPrintStatus("   Data format: %s\n", format_type);
        import_envi(inDataName, inMetaName, unscaledBaseName);
    }
    /* Ingest ESRI format data */
    else if (strncmp(format_type, "ESRI", 4) == 0) {
        asfPrintStatus("   Data format: %s\n", format_type);
        import_esri(inDataName, inMetaName, unscaledBaseName);
    }
    /* Ingest Vexcel Sky Telemetry Format (STF) data */
    else if (strncmp(format_type, "STF", 3) == 0) {
        asfPrintStatus("   Data format: %s\n", format_type);
        if (inMetaNameOption == NULL)
            require_stf_pair(inBaseName, inDataName, inMetaName);
        else {
            /* Allow the base name to be different for data & meta */
            require_stf_data(inBaseName, inDataName);
            require_stf_metadata(inMetaNameOption, inMetaName);
        }
        int lat_constrained = upperLat != -99 && lowerLat != -99;
        import_stf(inDataName, inMetaName, unscaledBaseName, radiometry,
                   lat_constrained, lowerLat, upperLat, prcPath);
    }
    else if ( strncmp (format_type, "GEOTIFF", 7) == 0 ) {
      asfPrintStatus("   Data format: %s\n", format_type);
      GString *inGeotiffName = find_geotiff_name (inBaseName);
      if ( inGeotiffName == NULL ) {
	asfPrintError ("Couldn't find a GeoTIFF file (i.e. a file with "
		       "extension '.tif', '.tiff', '.TIF', or '.TIFF') "
		       "corresponding to specified inBaseName");
      }
      // At the moment, we are set up to ingest only a GeoTIFF
      // variants, and no general GeoTIFF handler is implemented
      // (rationale: it would stand an excellent chance of not working
      // on a random flavor we haven't tested).  The idea is to keep
      // adding flavors as needed by teaching detect_geotiff_flavor
      // about them and adding a specialized inport functions capable
      // of ingesting them.  Eventually we might feel comfortable
      // taking a crack at a catch-all importer, but I think its
      // probably better just to have a few generalized detectable
      // flavors at the end of detect_geotiff_flavor, and for the
      // really weird stuff go ahead and throw an exception.
      geotiff_importer importer = detect_geotiff_flavor (inGeotiffName->str);
      if ( importer != NULL ) {
        if (importer == import_arcgis_geotiff     &&
            strlen(image_data_type)               &&
            strncmp(image_data_type, "???", 3) != 0
           )
        {
          // NOTE: The following importers are declared with ", ..." for a
          // variable number of arguments, but only import_arcgis_geotiff()
          // uses an extra argument:
          //
          // import_arcgis_geotiff()
          // import_usgs_seamless()
          // import_asf_utm_geotiff()
          //
          // NOTE: Any of these three importers may be returned by
          // the detect_geotiff_flavor() function above
          //
          importer (inGeotiffName->str, outBaseName, image_data_type);
        }
        else {
          importer (inGeotiffName->str, outBaseName);
        }
      } else {
//	asfPrintWarning ("Couldn't identify the flavor of the GeoTIFF, "
//			 "falling back to the generic GeoTIFF importer (cross "
//			 "fingers)... \n");
	// Haven't written import-generic_geotiff yet...
	
        asfPrintError ("\nTried to import a GeoTIFF of unrecognized flavor\n\n");
	//import_generic_geotiff (inGeotiffName->str, outBaseName);
      }
      g_string_free (inGeotiffName, TRUE);
    }
    /* Don't recognize this data format; report & quit */
    else {
        asfPrintError("Unrecognized data format: '%s'",format_type);
    }

    /* resample, if necessary */
    if (do_resample)
    {
        if (range_scale < 0)
	    range_scale = DEFAULT_RANGE_SCALE;

        if (azimuth_scale < 0)
	    azimuth_scale = get_default_azimuth_scale(unscaledBaseName);

        asfPrintStatus("Resampling with scale factors: "
		       "%lf range, %lf azimuth.\n",
            range_scale, azimuth_scale);

        resample_nometa(unscaledBaseName, outBaseName,
			range_scale, azimuth_scale);

	asfPrintStatus("Done.\n");
    }

    /* metadata pixel size fix, if necessary */
    if (do_metadata_fix)
    {
        if (correct_y_pixel_size < 0)
	    correct_y_pixel_size = get_default_ypix(outBaseName);

        fix_ypix(outBaseName, correct_y_pixel_size);
    }

    /* If the user asked for sprocket layers, create sprocket data layers
    now that we've got asf tools format data */
    //if (flags[f_SPROCKET] != FLAG_NOT_SET) {
    //    create_sprocket_layers(outBaseName, inMetaName);
    //}
    
    asfPrintStatus("Import complete.\n");
    return 0;
}
