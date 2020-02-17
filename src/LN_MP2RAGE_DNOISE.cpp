
#include "./common.h"
#include "./utils.h"

int show_help(void) {
    printf(
    "LN_MP2RAGE_DNOISE : Denoising MP2RAGE data.\n"
    "\n"
    "    This program removes some of the background noise in MP2RAGE, \n"
    "    UNI images to make themn look like MPRAGE images. This is done \n"
    "    without needing to have the phase information. This is following \n"
    "    the paper O’Brien KR et al. (2014) Robust T1-Weighted Structural \n"
    "    Brain Imaging and Morphometry at 7T Using MP2RAGE. PLoS ONE 9(6): \n"
    "    e99676. <doi:10.1371/journal.pone.0099676> \n"
    "\n"
    "Usage:\n"
    "    LN_MP2RAGE_DNOISE -INV1 INV1.nii -INV2 INV2.nii -UNI UNI.nii -beta 0.2\n"
    "\n"
    "Options\n"
    "    -help       : Show this help.\n"
    "    -INV1       : Nifti (.nii) file of the first inversion time.\n"
    "    -INV2       : Nifti (.nii) file of the second inversion time.\n"
    "    -UNI        : Nifti (.nii) of MP2RAGE UNI. Expecting SIEMENS \n"
    "                  values between 0-4095. \n"
    "    -beta value : Regularization term. Default is 0.2.\n"
    "    -output     : (Optional) Custom output name. \n"
    "\n"
    "Note: This program supports INT16, INT32 and FLOAT32. \n"
    "\n");
    return 0;
}

int main(int argc, char* argv[]) {
    float SIEMENS_f = 4095.0;
    char* fmaski = NULL, *fout = NULL, *finfi_1 = NULL, *finfi_2 = NULL;
    char* finfi_3 = NULL;
    int ac, custom_output = 0;
    float beta = 0.2;
    if (argc < 3) return show_help();  // Typing '-help' is sooo much work

    // Process user options: 4 are valid presently
    for (ac = 1; ac < argc; ac++) {
        if (!strncmp(argv[ac], "-h", 2)) {
            return show_help();
        } else if (!strcmp(argv[ac], "-beta")) {
            if (++ac >= argc) {
                fprintf(stderr, "** missing argument for -betan");
                return 1;
            }
            beta = atof(argv[ac]);
            // cout << " I will do gaussian temporal smoothing " << endl;
        } else if (!strcmp(argv[ac], "-INV1")) {
            if (++ac >= argc) {
                fprintf(stderr, "** missing argument for -INV1\n");
                return 1;
            }
            finfi_1 = argv[ac];
        } else if (!strcmp(argv[ac], "-INV2")) {
            if (++ac >= argc) {
                fprintf(stderr, "** missing argument for -INV2\n");
                return 1;
            }
            finfi_2 = argv[ac];
        } else if (!strcmp(argv[ac], "-UNI")) {
            if (++ac >= argc) {
                fprintf(stderr, "** missing argument for -UNI\n");
                return 1;
            }
            finfi_3 = argv[ac];
        } else if (!strcmp(argv[ac], "-output")) {
            if (++ac >= argc) {
                fprintf(stderr, "** missing argument for -output\n");
                return 1;
            }
            custom_output = 1;
            fout = argv[ac];
        } else {
            fprintf(stderr, "** invalid option, '%s'\n", argv[ac]);
            return 1;
        }
    }

    if (!finfi_1) {
        fprintf(stderr, "** missing option '-INV1'\n");
        return 1;
    }
    if (!finfi_2) {
        fprintf(stderr, "** missing option '-INV2'\n");
        return 1;
    }
    if (!finfi_3) {
        fprintf(stderr, "** missing option '-UNI '\n");
        return 1;
    }

    // Read input dataset, including data
    nifti_image* nim_inputfi_1 = nifti_image_read(finfi_1, 1);
    if (!nim_inputfi_1) {
        fprintf(stderr, "** failed to read NIfTI from '%s'\n", finfi_1);
        return 2;
    }

    nifti_image* nim_inputfi_2 = nifti_image_read(finfi_2, 1);
    if (!nim_inputfi_2) {
        fprintf(stderr, "** failed to read NIfTI from '%s'\n", finfi_2);
        return 2;
    }

    nifti_image* nim_inputfi_3 = nifti_image_read(finfi_3, 1);
    if (!nim_inputfi_3) {
        fprintf(stderr, "** failed to read NIfTI from '%s'\n", finfi_3);
        return 2;
    }

    log_welcome("LN_MP2RAGE_DNOISE");
    log_nifti_descriptives(nim_inputfi_1);
    log_nifti_descriptives(nim_inputfi_2);
    log_nifti_descriptives(nim_inputfi_3);

    // Get dimensions of input
    int size_x = nim_inputfi_1->nx;  // phase
    int size_y = nim_inputfi_1->ny;  // read
    int size_z = nim_inputfi_1->nz;  // slice
    int size_t = nim_inputfi_1->nt;  // time
    int nx = nim_inputfi_1->nx;
    int nxy = nim_inputfi_1->nx * nim_inputfi_1->ny;
    int nxyz = nim_inputfi_1->nx * nim_inputfi_1->ny * nim_inputfi_1->nz;

    //////////////////
    // LOADING INV1 //
    //////////////////
    nifti_image* nim_inv1 = nifti_copy_nim_info(nim_inputfi_1);
    nim_inv1->datatype = NIFTI_TYPE_FLOAT32;
    nim_inv1->nbyper = sizeof(float);
    nim_inv1->data = calloc(nim_inv1->nvox, nim_inv1->nbyper);
    float* nim_inv1_data = static_cast<float*>(nim_inv1->data);

    //////////////////////////////////////////////////////////////
    // Fixing potential problems with different input datatypes //
    // here, I am loading them in their native datatype         //
    // and translate them to the datatime I like best           //
    //////////////////////////////////////////////////////////////

    if (nim_inputfi_3->datatype == NIFTI_TYPE_FLOAT32 ||
        nim_inputfi_3->datatype == NIFTI_TYPE_INT32) {
        float* nim_inputfi_1_data = static_cast<float*>(nim_inputfi_1->data);
        FOR_EACH_VOXEL_TZYX
            *(nim_inv1_data + VOXEL_ID) =
                static_cast<float>(*(nim_inputfi_1_data + VOXEL_ID));
        END_FOR_EACH_VOXEL_TZYX
    }

    if (nim_inputfi_3->datatype == NIFTI_TYPE_INT16 ||
        nim_inputfi_3->datatype == DT_UINT16) {
        int16_t* nim_inputfi_1_data =
            static_cast<int16_t*>(nim_inputfi_1->data);
        FOR_EACH_VOXEL_TZYX
            *(nim_inv1_data + VOXEL_ID) =
                static_cast<float>(*(nim_inputfi_1_data + VOXEL_ID));
        END_FOR_EACH_VOXEL_TZYX
    }

    //////////////////
    // Loading INV2 //
    //////////////////
    nifti_image* nim_inv2 = nifti_copy_nim_info(nim_inputfi_2);
    nim_inv2->datatype = NIFTI_TYPE_FLOAT32;
    nim_inv2->nbyper = sizeof(float);
    nim_inv2->data = calloc(nim_inv2->nvox, nim_inv2->nbyper);
    float*nim_inv2_data = static_cast<float*>(nim_inv2->data);

    //////////////////////////////////////////////////////////////
    // Fixing potential problems with different input datatypes //
    // again /////////////////////////////////////////////////////
    //////////////////////////////////////////////////////////////
    if (nim_inputfi_3->datatype == NIFTI_TYPE_FLOAT32 ||
        nim_inputfi_3->datatype == NIFTI_TYPE_INT32) {
        float* nim_inputfi_2_data = static_cast<float*>(nim_inputfi_2->data);
        FOR_EACH_VOXEL_TZYX
            *(nim_inv2_data + VOXEL_ID) =
                static_cast<float>(*(nim_inputfi_2_data + VOXEL_ID));
        END_FOR_EACH_VOXEL_TZYX
    }

    if (nim_inputfi_3->datatype == NIFTI_TYPE_INT16 ||
        nim_inputfi_3->datatype == DT_UINT16) {
        int16_t* nim_inputfi_2_data =
            static_cast<int16_t*>(nim_inputfi_2->data);
        FOR_EACH_VOXEL_TZYX
            *(nim_inv2_data + VOXEL_ID) =
                static_cast<float>(*(nim_inputfi_2_data + VOXEL_ID));
        END_FOR_EACH_VOXEL_TZYX
    }

    /////////////////
    // LOADING UNI //
    /////////////////
    nifti_image* nim_uni = nifti_copy_nim_info(nim_inputfi_3);
    nim_uni->datatype = NIFTI_TYPE_FLOAT32;
    nim_uni->nbyper = sizeof(float);
    nim_uni->data = calloc(nim_uni->nvox, nim_uni->nbyper);
    float* nim_uni_data = static_cast<float*>(nim_uni->data);

    ////////////////////////////////////////////////////////////////////
    // Fixing potential problems with different input datatypes again //
    ////////////////////////////////////////////////////////////////////
    if (nim_inputfi_3->datatype == NIFTI_TYPE_FLOAT32 ||
        nim_inputfi_3->datatype == NIFTI_TYPE_INT32) {
        float* nim_inputfi_3_data = static_cast<float*>(nim_inputfi_3->data);
        FOR_EACH_VOXEL_TZYX
            *(nim_uni_data + VOXEL_ID) =
            static_cast<float>(*(nim_inputfi_3_data + VOXEL_ID));
        END_FOR_EACH_VOXEL_TZYX
    }

    if (nim_inputfi_3->datatype == NIFTI_TYPE_INT16 ||
        nim_inputfi_3->datatype == DT_UINT16) {
        int16_t* nim_inputfi_3_data =
            static_cast<int16_t*>(nim_inputfi_3->data);
        FOR_EACH_VOXEL_TZYX
            *(nim_uni_data + VOXEL_ID) =
                static_cast<float>(*(nim_inputfi_3_data + VOXEL_ID));
        END_FOR_EACH_VOXEL_TZYX
    }

    /////////////////////////////////////
    // Make allocating necessary files //
    /////////////////////////////////////
    nifti_image* phaseerror = nifti_copy_nim_info(nim_inv1);
    nifti_image* dddenoised = nifti_copy_nim_info(nim_inv1);
    phaseerror->datatype = NIFTI_TYPE_FLOAT32;
    dddenoised->datatype = NIFTI_TYPE_FLOAT32;
    phaseerror->nbyper = sizeof(float);
    dddenoised->nbyper = sizeof(float);
    phaseerror->data = calloc(phaseerror->nvox, phaseerror->nbyper);
    dddenoised->data = calloc(dddenoised->nvox, dddenoised->nbyper);
    float* phaseerror_data = static_cast<float*>(phaseerror->data);
    float* dddenoised_data = static_cast<float*>(dddenoised->data);

    nifti_image* uni1  = nifti_copy_nim_info(nim_inv1);
    nifti_image* uni2  = nifti_copy_nim_info(nim_inv1);
    uni1->datatype = NIFTI_TYPE_FLOAT32;
    uni2->datatype = NIFTI_TYPE_FLOAT32;
    uni1->nbyper = sizeof(float);
    uni2->nbyper = sizeof(float);
    uni1->data = calloc(uni1->nvox, uni1->nbyper);
    uni2->data = calloc(uni2->nvox, uni2->nbyper);
    // float* uni1_data = static_cast<float*>(uni1->data);
    // float* uni2_data = static_cast<float*>(uni2->data);

    ///////////////////////////////////////
    // Big calculation across all voxels //
    ///////////////////////////////////////
    // float sign_ = 0;

    beta = beta * SIEMENS_f;

    float inv2val = 0;
    // float inv1val = 0;
    float unival = 0;
    float wrong_unival = 0;

    float uni1val_calc = 0;
    float uni2val_calc = 0;
    // float denoised_wrong = 0;

    /////////////////////////////////////////////////////////
    // Scaling UNI to range of -0.5 to 0.5 as in the paper //
    /////////////////////////////////////////////////////////
    FOR_EACH_VOXEL_TZYX
        unival = (*(nim_uni_data + VOXEL_ID) - SIEMENS_f * 0.5) / SIEMENS_f;
        // inv1val = *(nim_inv1_data + VOXEL_ID);
        inv2val = *(nim_inv2_data + VOXEL_ID);
        wrong_unival = *(nim_inv1_data + VOXEL_ID) * *(nim_inv2_data + VOXEL_ID) / (*(nim_inv1_data + VOXEL_ID)* *(nim_inv1_data + VOXEL_ID) + *(nim_inv2_data + VOXEL_ID)* *(nim_inv2_data + VOXEL_ID));

        // sign_ = unival;
        // *(nim_uni_data + VOXEL_ID) / *(phaseerror_data + VOXEL_ID);
        // if (sign_ <= 0) {
        //     *(nim_inv1_data + VOXEL_ID) = -1 * *(nim_inv1_data + VOXEL_ID);
        // }

        // denoised_wrong = (*(nim_inv1_data + VOXEL_ID) * *(nim_inv2_data + VOXEL_ID) - beta) / (*(nim_inv1_data + VOXEL_ID) * *(nim_inv1_data + VOXEL_ID) + *(nim_inv2_data + VOXEL_ID) * *(nim_inv2_data + VOXEL_ID) + 2. * beta);
        // denoised_wrong = (denoised_wrong +0.5) * SIEMENS_f;
        *(phaseerror_data + VOXEL_ID) = wrong_unival;

        uni1val_calc = inv2val * (1 / (2 * unival) + sqrt(1 / (4 * unival * unival) - 1));
        uni2val_calc = inv2val * (1 / (2 * unival) - sqrt(1 / (4 * unival * unival) - 1));

        if (unival > 0) {
            uni1val_calc = uni2val_calc;
        }

        // if (!(uni1val_calc > SIEMENS_f || uni1val_calc < SIEMENS_f)) uni1val_calc = inv1val;

        // *(uni1_data + VOXEL_ID) = uni1val_calc;
        // *(uni2_data + VOXEL_ID) = uni2val_calc;
        // *(phaseerror_data + VOXEL_ID) = unival;

        *(dddenoised_data + VOXEL_ID) = ((uni1val_calc * inv2val - beta) / (uni1val_calc * uni1val_calc + inv2val * inv2val + 2. * beta) + 0.5) * SIEMENS_f;
    END_FOR_EACH_VOXEL_TZYX

    dddenoised->scl_slope = nim_uni->scl_slope;

    if (nim_uni->scl_inter != 0) {
        cout << " ########################################## " << endl;
        cout << " #####   WARNING   WANRING   WANRING  ##### " << endl;
        cout << " ## the NIFTI scale factor is asymmetric ## " << endl;
        cout << " ## Why would you do such a thing????    ## " << endl;
        cout << " #####   WARNING   WANRING   WANRING  ##### " << endl;
        cout << " ########################################## " << endl;
    }

    if (custom_output == 1) {
        string outfilename = (string) (fout);
        log_output(outfilename.c_str());
        const char* fout_1 = outfilename.c_str();
        if (nifti_set_filenames(dddenoised, fout_1, 1, 1)) {
            return 1;
        }
    } else {
        string prefix = "denoised_";
        string filename = (string) (finfi_3);
        string outfilename = prefix+filename;
        log_output(outfilename.c_str());
        const char* fout_1 = outfilename.c_str();
        if (nifti_set_filenames(dddenoised, fout_1, 1, 1)) {
            return 1;
        }
    }

    nifti_image_write(dddenoised);

    const char* fout_2 = "Border_enhance.nii";
    if (nifti_set_filenames(phaseerror, fout_2, 1, 1)) {
        return 1;
    }
    nifti_image_write(phaseerror);

    cout << "  Finished." << endl;
    return 0;
}
