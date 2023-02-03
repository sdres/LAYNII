#include "../dep/laynii_lib.h"
#include <sstream>

int show_help(void) {
    printf(
    "LN2_GRAMAG: Compute gradient magnitude image.\n"
    "\n"
    "Usage:\n"
    "    LN2_GRAMAG -input input.nii\n"
    "    ../LN2_GRAMAG -input input.nii\n"
    "\n"
    "Options:\n"
    "    -help   : Show this help.\n"
    "    -input  : Nifti image with values that will be used to compute gradients.\n"
    "    -output : (Optional) Output basename for all outputs.\n"
    "\n"
    "Reference / further reading:\n"
    "    [See Figure 1 from] Gulban, O.F., Schneider, M., Marquardt, I., \n"
    "    Haast, R.A.M., De Martino, F., 2018. A scalable method to improve \n"
    "    gray matter segmentation at ultra high field MRI. PloS one 13, e0198335.\n"
    "    <https://doi.org/10.1371/journal.pone.0198335>\n"
    "\n");
    return 0;
}

int main(int argc, char*  argv[]) {

    nifti_image *nii1 = NULL;
    char *fin1 = NULL, *fout = NULL;
    int ac;

    // Process user options
    if (argc < 2) return show_help();
    for (ac = 1; ac < argc; ac++) {
        if (!strncmp(argv[ac], "-h", 2)) {
            return show_help();
        } else if (!strcmp(argv[ac], "-input")) {
            if (++ac >= argc) {
                fprintf(stderr, "** missing argument for -input\n");
                return 1;
            }
            fin1 = argv[ac];
            fout = argv[ac];
        } else if (!strcmp(argv[ac], "-output")) {
            if (++ac >= argc) {
                fprintf(stderr, "** missing argument for -output\n");
                return 1;
            }
            fout = argv[ac];
        } else {
            fprintf(stderr, "** invalid option, '%s'\n", argv[ac]);
            return 1;
        }
    }

    if (!fin1) {
        fprintf(stderr, "** missing option '-input'\n");
        return 1;
    }

    // Read input dataset, including data
    nii1 = nifti_image_read(fin1, 1);
    if (!nii1) {
        fprintf(stderr, "** failed to read NIfTI from '%s'\n", fin1);
        return 2;
    }

    log_welcome("LN2_GRAMAG");
    log_nifti_descriptives(nii1);

    // Get dimensions of input
    const uint32_t size_x = nii1->nx;
    const uint32_t size_y = nii1->ny;
    const uint32_t size_z = nii1->nz;

    const uint32_t end_x = size_x - 1;
    const uint32_t end_y = size_y - 1;
    const uint32_t end_z = size_z - 1;

    const uint32_t nr_voxels = size_z * size_y * size_x;

    // ========================================================================
    // Fix input datatype issues
    // ========================================================================
    nifti_image* nii_input = copy_nifti_as_float32(nii1);
    float* nii_input_data = static_cast<float*>(nii_input->data);

    // Prepare output image
    nifti_image* nii_gramag = copy_nifti_as_float32(nii_input);
    float* nii_gramag_data = static_cast<float*>(nii_gramag->data);

    // Set to zero
    for (uint32_t i = 0; i != nr_voxels; ++i) {
        *(nii_gramag_data + i) = 0;
    }

    // ========================================================================
    // Find connected clusters
    // ========================================================================
    cout << "  Computing gradients..." << endl;

    uint32_t ix, iy, iz, j, k;
    for (uint32_t i = 0; i != nr_voxels; ++i) {
        tie(ix, iy, iz) = ind2sub_3D(i, size_x, size_y);
        float gra_x = 0, gra_y = 0, gra_z = 0;
        // float g21 = 0, g22 = 0, g23 = 0, g24 = 0, g25 = 0, g26 = 0;
        // float g31 = 0, g32 = 0, g33 = 0, g34 = 0;

        // --------------------------------------------------------------------
        // 1-jump neighbours
        // --------------------------------------------------------------------
        if (ix > 0 && ix < end_x) {
            j = sub2ind_3D(ix-1, iy, iz, size_x, size_y);
            k = sub2ind_3D(ix+1, iy, iz, size_x, size_y);
            gra_x += (*(nii_input_data + j) - *(nii_input_data + k)) / 2;
        }
        if (iy > 0 && iy < end_y) {
            j = sub2ind_3D(ix, iy-1, iz, size_x, size_y);
            k = sub2ind_3D(ix, iy+1, iz, size_x, size_y);
            gra_y += (*(nii_input_data + j) - *(nii_input_data + k)) / 2;
        }
        if (iz > 0 && iz < end_z) {
            j = sub2ind_3D(ix, iy, iz-1, size_x, size_y);
            k = sub2ind_3D(ix, iy, iz+1, size_x, size_y);
            gra_z += (*(nii_input_data + j) - *(nii_input_data + k)) / 2;
        }

        // // --------------------------------------------------------------------
        // // 2-jump neighbours
        // // --------------------------------------------------------------------
        // if (ix > 0 && iy > 0 && ix < end_x && iy < end_y) {
        //     j = sub2ind_3D(ix-1, iy-1, iz, size_x, size_y);
        //     k = sub2ind_3D(ix+1, iy+1, iz, size_x, size_y);
        //     g21 += (*(nii_input_data + j) - *(nii_input_data + k)) / (2 * 1.4142136);
        // }
        // if (ix > 0 && iy < end_y && ix < end_x && iy > 0) {
        //     j = sub2ind_3D(ix-1, iy+1, iz, size_x, size_y);
        //     k = sub2ind_3D(ix+1, iy-1, iz, size_x, size_y);
        //     g22 += (*(nii_input_data + j) - *(nii_input_data + k)) / (2 * 1.4142136);
        // }
        // if (iy > 0 && iz > 0 && iy < end_y && iz < end_z) {
        //     j = sub2ind_3D(ix, iy-1, iz-1, size_x, size_y);
        //     k = sub2ind_3D(ix, iy+1, iz+1, size_x, size_y);
        //     g23 += (*(nii_input_data + j) - *(nii_input_data + k)) / (2 * 1.4142136);
        // }
        // if (iy > 0 && iz < end_z && iy < end_y && iz > 0) {
        //     j = sub2ind_3D(ix, iy-1, iz+1, size_x, size_y);
        //     k = sub2ind_3D(ix, iy+1, iz-1, size_x, size_y);
        //     g24 += (*(nii_input_data + j) - *(nii_input_data + k)) / (2 * 1.4142136);
        // }
        // if (ix > 0 && iz > 0 && ix < end_x && iz < end_z) {
        //     j = sub2ind_3D(ix-1, iy, iz-1, size_x, size_y);
        //     k = sub2ind_3D(ix+1, iy, iz+1, size_x, size_y);
        //     g25 += (*(nii_input_data + j) - *(nii_input_data + k)) / (2 * 1.4142136);
        // }
        // if (ix < end_x && iz > 0 && ix > 0 && iz < end_z) {
        //     j = sub2ind_3D(ix+1, iy, iz-1, size_x, size_y);
        //     k = sub2ind_3D(ix-1, iy, iz+1, size_x, size_y);
        //     g26 += (*(nii_input_data + j) - *(nii_input_data + k)) / (2 * 1.4142136);
        // }
        //
        // // --------------------------------------------------------------------
        // // 3-jump neighbours
        // // --------------------------------------------------------------------
        // if (ix > 0 && iy > 0 && iz > 0 && ix < end_x && iy < end_y && iz < end_z) {
        //     j = sub2ind_3D(ix-1, iy-1, iz-1, size_x, size_y);
        //     k = sub2ind_3D(ix+1, iy+1, iz+1, size_x, size_y);
        //     g31 += (*(nii_input_data + j) - *(nii_input_data + k)) / (2 * 1.7320508);
        // }
        // if (ix > 0 && iy > 0 && iz < end_z && ix < end_x && iy < end_y && iz > 0) {
        //     j = sub2ind_3D(ix-1, iy-1, iz+1, size_x, size_y);
        //     k = sub2ind_3D(ix+1, iy+1, iz-1, size_x, size_y);
        //     g32 += (*(nii_input_data + j) - *(nii_input_data + k)) / (2 * 1.7320508);
        // }
        // if (ix > 0 && iy < end_y && iz > 0 && ix < end_x && iy > 0 && iz < end_z) {
        //     j = sub2ind_3D(ix-1, iy+1, iz-1, size_x, size_y);
        //     k = sub2ind_3D(ix+1, iy-1, iz+1, size_x, size_y);
        //     g33 += (*(nii_input_data + j) - *(nii_input_data + k)) / (2 * 1.7320508);
        // }
        // if (ix < end_x && iy > 0 && iz > 0 && ix > 0 && iy < end_y && iz < end_z) {
        //     j = sub2ind_3D(ix+1, iy-1, iz-1, size_x, size_y);
        //     k = sub2ind_3D(ix-1, iy+1, iz+1, size_x, size_y);
        //     g34 += (*(nii_input_data + j) - *(nii_input_data + k)) / (2 * 1.7320508);
        // }

        // --------------------------------------------------------------------
        // Compute magnitude
        *(nii_gramag_data + i) = sqrt(gra_x*gra_x + gra_y*gra_y + gra_z*gra_z);
        // *(nii_gramag_data + i) = sqrt(g21*g21 + g22*g22 + g23*g23 + g24*g24 + g25*g25 + g26*g26);
        // *(nii_gramag_data + i) = sqrt(g31*g31 + g32*g32 + g33*g33 + g34*g34);
    }

    cout << "  Saving output..." << endl;
    save_output_nifti(fout, "gramag", nii_gramag, true);

    cout << "\n  Finished." << endl;
    return 0;
}
