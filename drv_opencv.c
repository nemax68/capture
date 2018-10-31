



#ifdef CV
static void dump_image(const void *p, int size)
{
	unsigned char *outdata = (unsigned char *) p;
	struct jpeg_compress_struct cinfo;
	struct jpeg_error_mgr jerr;
	unsigned char *outbuf = NULL;
	long unsigned int outbuf_size = 0;
	unsigned char row_buf[4096];
    JSAMPROW row_pointer[1];
    clock_t begin,end;
    unsigned i, j;
	unsigned offset;

    begin = clock();

	cinfo.err = jpeg_std_error(&jerr);

	jpeg_create_compress(&cinfo);
	cinfo.image_width = img_width;
	cinfo.image_height = img_height;
	cinfo.input_components = 3;
	cinfo.in_color_space = JCS_YCbCr; // input color space

	jpeg_mem_dest(&cinfo, &outbuf, &outbuf_size);
	jpeg_set_defaults(&cinfo);
	cinfo.dct_method = JDCT_IFAST; // DCT method
	jpeg_set_quality(&cinfo, 40, TRUE);
	jpeg_start_compress(&cinfo, TRUE);

	if(outbuf==NULL)
	{
		fprintf(stderr, "jpeg_mem_dest:no memory\n");
		exit(EXIT_FAILURE);
	}

	row_stride = img_width *2;// frame->nChannels;
    row_pointer[0] = row_buf;

	while (cinfo.next_scanline < cinfo.image_height) {
		/* jpeg_write_scanlines expects an array of pointers to scanlines.
		* Here the array is only one element long, but you could pass
		* more than one scanline at a time if that's more convenient.
		*/

		offset = cinfo.next_scanline * cinfo.image_width * 2;

		for (i = 0, j = 0; i < cinfo.image_width*2; i += 4, j += 6) {
		        row_buf[j + 0] = outdata[offset + i + 0]; // Y
		        row_buf[j + 1] = outdata[offset + i + 1]; // U
		        row_buf[j + 2] = outdata[offset + i + 3]; // V
		        row_buf[j + 3] = outdata[offset + i + 2]; // Y
		        row_buf[j + 4] = outdata[offset + i + 1]; // U
		        row_buf[j + 5] = outdata[offset + i + 3]; // V
		}

		//row_ptr[0] = &outdata[cinfo.next_scanline * row_stride];

		(void) jpeg_write_scanlines(&cinfo, row_pointer, 1);
	}

	jpeg_finish_compress(&cinfo);

	end = clock();

	printf("Compress len=%ld in %ld us\n",outbuf_size,(end-begin));

	i=sendto(sockfd, outbuf, outbuf_size, 0, (struct sockaddr*)&serv_addr, slen);

	printf("Compress len=%ld in %ld us tx %d bytes\n",outbuf_size,(end-begin),i);

	jpeg_destroy_compress(&cinfo);

	frame_number++;

}
#endif

