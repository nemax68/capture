#ifdef notdef

/* a selection of JPEG markers */
#define TEM				0x01
#define SOF0				0xc0
#define RST				0xd0
#define SOI				0xd8
#define EOI				0xd9
#define DHP				0xde

typedef unsigned long    u32;

/**
 * s5p_jpeg_buffer - description of memory containing input JPEG data
 * @size:	buffer size
 * @curr:	current position in the buffer
 * @data:	pointer to the data
 */
struct s5p_jpeg_buffer {
	unsigned long size;
	unsigned long curr;
	unsigned long data;
};

/**
 * s5p_jpeg_q_data - parameters of one queue
 * @fmt:	driver-specific format of this queue
 * @w:		image width
 * @h:		image height
 * @size:	image buffer size in bytes
 */
struct s5p_jpeg_q_data {
	struct s5p_jpeg_fmt	*fmt;
	u32			w;
	u32			h;
	u32			size;
};

static int get_byte(struct s5p_jpeg_buffer *buf)
{
	if (buf->curr >= buf->size)
		return -1;

	return ((unsigned char *)buf->data)[buf->curr++];
}

static int get_word_be(struct s5p_jpeg_buffer *buf, unsigned int *word)
{
	unsigned int temp;
	int byte;

	byte = get_byte(buf);
	if (byte == -1)
		return -1;
	temp = byte << 8;
	byte = get_byte(buf);
	if (byte == -1)
		return -1;
	*word = (unsigned int)byte | temp;
	return 0;
}

static void skip(struct s5p_jpeg_buffer *buf, long len)
{
	if (len <= 0)
		return;

	while (len--)
		get_byte(buf);
}


static char s5p_jpeg_parse_hdr(struct s5p_jpeg_q_data *result,
			       unsigned long buffer, unsigned long size)
{
	int c, components, notfound;
	unsigned int height, width, word;
	long length;
	struct s5p_jpeg_buffer jpeg_buffer;

	jpeg_buffer.size = size;
	jpeg_buffer.data = buffer;
	jpeg_buffer.curr = 0;

	notfound = 1;
	while (notfound) {
		c = get_byte(&jpeg_buffer);
		if (c == -1)
			break;
		if (c != 0xff)
			continue;
		do
			c = get_byte(&jpeg_buffer);
		while (c == 0xff);
		if (c == -1)
			break;
		if (c == 0)
			continue;
		length = 0;
		switch (c) {
		/* SOF0: baseline JPEG */
		case SOF0:
			if (get_word_be(&jpeg_buffer, &word))
				break;
			if (get_byte(&jpeg_buffer) == -1)
				break;
			if (get_word_be(&jpeg_buffer, &height))
				break;
			if (get_word_be(&jpeg_buffer, &width))
				break;
			components = get_byte(&jpeg_buffer);

			if (components == -1)
				break;

			//skip(&jpeg_buffer, components * 3);
			skip(&jpeg_buffer, word-8);
			break;

		/* skip payload-less markers */
		case RST ... RST + 7:
			break;
		case SOI:
			break;
		case EOI:
			notfound = 0;
			break;
		case TEM:
			break;

		/* skip uninteresting payload markers */
		default:
			if (get_word_be(&jpeg_buffer, &word))
				break;
			length = (long)word - 2;
			skip(&jpeg_buffer, length);
			break;
		}
	}
	result->w = width;
	result->h = height;
	result->size = jpeg_buffer.curr;
	return !notfound;
}


#define JPEG_PADDING 0x55
#define MIN_JPEG_LINE 2


int jpeg_size_extract(unsigned char *p, int hight,int witdh)
{
	int 			i,j,offset;
	unsigned char 	*outdata;

	for(j=MIN_JPEG_LINE;j<hight-1;j++){
		offset=(j*img_width);
		outdata = p + offset;
		if(*outdata==JPEG_PADDING)
		{
			i=0;
			while(*(++outdata)==JPEG_PADDING) i++;

			if(i>(witdh/2)) {
				/* trovato pattern JPEG_PADDING.......
				 * ricerca EOF codice 0xFFD9
				 */
				outdata = p + offset;
				while( !((*(outdata)==0xFF) && (*(outdata+1)==0xD9)) )
				{
					if(--offset==0)	return(0);
					outdata--;
				}
				break;
			}
		}
	}

	return(offset+2);

}

#endif
