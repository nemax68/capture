/*
 *  V4L2 video capture example
 *
 *  This program can be used and distributed without restrictions.
 *
 *      This program is provided with the V4L2 API
 * see http://linuxtv.org/docs.php for more information
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <getopt.h>             /* getopt_long() */

#include <fcntl.h>              /* low-level i/o */
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/ioctl.h>

#include <linux/videodev2.h>

#include <jpeglib.h>

#include <arpa/inet.h>
#include <netinet/in.h>

#include <termios.h>

#include <sys/select.h>
#include <stropts.h>

#include <opencv2/objdetect.hpp>

extern int xioctl(int fh, int request, void *arg);

extern int set_jpeg_quality(int fd, int quality);
extern int set_frame_rate(int fd, int rate);
extern int zoom_absolute(int fd, int zoom);
extern int zoom_relative(int fd, int zoom);
extern int pan_relative(int fd, int pan);
extern int pan_absolute(int fd, int pan);
extern int tilt_relative(int fd, int tilt);
extern int tilt_absolute(int fd, int tilt);

extern void errno_exit(const char *s);

extern int kbhit(void);
extern int getch(void);

#define CLEAR(x) memset(&(x), 0, sizeof(x))

#define MAX_PACKET_SIZE		1024

//#define VIDEO_WIDTH		320
//#define VIDEO_HIGHT		240

#define JPEG_SIZE_IN_DRIVER
//#define NOSCAN


#define PORT 9930
//#define SPORT "9930"

//#ifndef V4L2_PIX_FMT_H264
//#define V4L2_PIX_FMT_H264     v4l2_fourcc('H', '2', '6', '4') /* H264 with start codes */
//#endif

#ifndef V4L2_PIX_FMT_VYUY
#define V4L2_PIX_FMT_VYUY     v4l2_fourcc('V', 'Y', 'U', 'Y') /* H264 with start codes */
#endif

enum io_method {
        IO_METHOD_READ,
        IO_METHOD_MMAP,
        IO_METHOD_USERPTR,
};

struct buffer {
        void   *start;
        size_t  length;
};

static char            			*dev_name;
static char 				*addr;//  "10.1.1.35";
static enum io_method   		io = IO_METHOD_MMAP;
static int             			fd = -1;
struct buffer          			*buffers;
static unsigned int     		n_buffers;
static int              		out_buf=0;
static int              		image_format;
static int              		image_type;
static int              		frame_count = 10000;
static int              		frame_number = 0;
static int              		zoom = 1;
static int              		img_height = 240;
static int              		img_width = 320;
static int 				tcp_conn=0;
static int 						sockfd;
static struct sockaddr_in 		serv_addr;
static int						slen=sizeof(serv_addr);
static int				ratio;
static int				max_pkt_size;
static int				jpeg_quality=50;
static int				img_jpeg=0;



static void process_image(const void *p, int size)
{
        frame_number++;
        char filename[15];
        sprintf(filename, "frame-%d.raw", frame_number);
        FILE *fp=fopen(filename,"wb");

        if (out_buf)
                fwrite(p, size, 1, fp);

        fflush(fp);
        fclose(fp);
}

#define FMT_320x240	0x1
#define FMT_640x480	0x2
#define FMT_800x600	0x3
#define FMT_XXX		0xF

#define TYP_YUV422	0x1
#define TYP_JPEG	0x2

struct vid_udp_head{
	uint8_t		code;
	uint8_t		id;
	uint16_t	frame;
	uint16_t	pkt;
	uint16_t	npkt;
	uint16_t	pkt_size;
	uint8_t		format_type;
	uint8_t		reserved;
	uint32_t 	image_size;
};

struct timeval 	time_hold;

static void jpeg_image(const void *p, int size)
{
	unsigned char 		*pt = (unsigned char *) p;
	struct	iovec 		iov[2];
	struct 	msghdr 		message;
	struct vid_udp_head 	head;
	struct timeval 		time_s,time_e;
	long 			time_elapsed;
	int	len;
	int	tlen=size;
	int 	ret;
	int 	npkt;
	int	i;

	frame_number++;

	gettimeofday(&time_s,NULL);

	time_elapsed=(time_s.tv_sec-time_hold.tv_sec)*1000000+time_s.tv_usec-time_hold.tv_usec;
	time_hold=time_s;
	printf("\nwrite on socket [%ld]",time_elapsed);


	if (out_buf)
		process_image(pt,size);
	else{
		npkt=tlen/max_pkt_size;
		if(tlen%max_pkt_size)
			npkt++;
		for(i=0;i<npkt;i++){
			len = (tlen>max_pkt_size)?max_pkt_size:tlen;
			tlen= tlen -len;

			head.pkt_size=len;
			head.code=0;
			head.id=0;
			head.frame=frame_number;
			head.pkt=i+1;
			head.npkt=npkt;
			head.image_size=size;
			head.format_type=((image_format<<4)&0xF0) | (image_type&0x0F);

			iov[0].iov_base=&head;
			iov[0].iov_len=sizeof(head);
			iov[1].iov_base=pt;
			iov[1].iov_len=len;

			pt=pt+len;

			message.msg_name=(struct sockaddr*)&serv_addr;
			message.msg_namelen=slen;
			message.msg_iov=iov;
			message.msg_iovlen=2;
			message.msg_control=0;
			message.msg_controllen=0;

			ret=sendmsg(sockfd,&message,0);

			if(ret>=0) printf("[%d]",len); else { printf("#"); break; }

		}
		//ret=sendto(sockfd, pt, size, 0, (struct sockaddr*)&serv_addr, slen);
	}
	gettimeofday(&time_e,NULL);
	time_elapsed=(time_e.tv_sec-time_s.tv_sec)*1000000+time_e.tv_usec-time_s.tv_usec;

	if(ret>=0)
		printf(" %d bytes in %ldus",size,time_elapsed);
	else{
		perror("write failed bytes");
	}
}


static int read_frame(void)
{
        struct v4l2_buffer buf;
#ifdef TEST_V4L2_CID_COLORFX
        struct v4l2_control ctrl;
#endif
        unsigned int i;

        switch (io) {
        case IO_METHOD_READ:
                if (-1 == read(fd, buffers[0].start, buffers[0].length)) {
                        switch (errno) {
                        case EAGAIN:
                                return 0;

                        case EIO:
                                /* Could ignore EIO, see spec. */

                                /* fall through */

                        default:
                                errno_exit("read");
                        }
                }
                printf("Rx image len=%d\n",buffers[0].length);
                //process_image(buffers[0].start, buffers[0].length);
                break;

        case IO_METHOD_MMAP:
        		CLEAR(buf);

                buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
                buf.memory = V4L2_MEMORY_MMAP;

                if (-1 == xioctl(fd, VIDIOC_DQBUF, &buf)) {
                        switch (errno) {
                        case EAGAIN:
                                return 0;

                        case EIO:
                                /* Could ignore EIO, see spec. */

                                /* fall through */

                        default:
                                errno_exit("VIDIOC_DQBUF");
                        }
                }

                assert(buf.index < n_buffers);
                //printf("\nxioct extract %d bytes used %d pt=%p",buf.length,buf.bytesused,buffers[buf.index].start);
                jpeg_image(buffers[buf.index].start, buf.bytesused);

                if (-1 == xioctl(fd, VIDIOC_QBUF, &buf))
                        errno_exit("VIDIOC_QBUF");

//#define TEST_ZOMM_PAN_TILT_ABS

#ifdef TEST_V4L2_CID_COLORFX
                if(frame_number>100)
                {
                	if(frame_number%50==0)
					{
						ctrl.id=V4L2_CID_COLORFX;
						ctrl.value=(frame_number-100)/50;

						if (-1 == xioctl(fd, VIDIOC_S_CTRL, &ctrl))
							errno_exit("VIDIOC_S_CTRL");
					}
                }
#endif

#ifdef TEST_ZOMM_PAN_TILT
                if(frame_number>100)
                {
					if(frame_number<400)
					{
						if(frame_number%10==0)
						{
							ctrl.id=V4L2_CID_ZOOM_RELATIVE;
							ctrl.value=10;

							if (-1 == xioctl(fd, VIDIOC_S_CTRL, &ctrl))
								errno_exit("VIDIOC_S_CTRL");
						}
					}
					else if(frame_number<500)
					{
						if(frame_number%10==0)
						{
							ctrl.id=V4L2_CID_PAN_RELATIVE;
							ctrl.value=5;

							if (-1 == xioctl(fd, VIDIOC_S_CTRL, &ctrl))
								errno_exit("VIDIOC_S_CTRL");
						}
					}
					else if(frame_number<600)
					{
						if(frame_number%10==0)
						{
							ctrl.id=V4L2_CID_TILT_RELATIVE;
							ctrl.value=5;

							if (-1 == xioctl(fd, VIDIOC_S_CTRL, &ctrl))
								errno_exit("VIDIOC_S_CTRL");
						}
					}
                }
#endif

#ifdef TEST_ZOMM_PAN_TILT_ABS
                if(frame_number==100)
				{
                	printf("\n###V4L2_CID_ZOOM_ABSOLUTE###");
					ctrl.id=V4L2_CID_ZOOM_ABSOLUTE;
					ctrl.value=400;

					if (-1 == xioctl(fd, VIDIOC_S_CTRL, &ctrl))
						errno_exit("VIDIOC_S_CTRL");
				}
                if(frame_number==200)
				{
					ctrl.id=V4L2_CID_PAN_ABSOLUTE;
					ctrl.value=0;

					if (-1 == xioctl(fd, VIDIOC_S_CTRL, &ctrl))
						errno_exit("VIDIOC_S_CTRL");
				}
                if(frame_number==201)
				{
					ctrl.id=V4L2_CID_TILT_ABSOLUTE;
					ctrl.value=0;

					if (-1 == xioctl(fd, VIDIOC_S_CTRL, &ctrl))
						errno_exit("VIDIOC_S_CTRL");
				}
#endif
                break;

        case IO_METHOD_USERPTR:
                CLEAR(buf);

                buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
                buf.memory = V4L2_MEMORY_USERPTR;

                if (-1 == xioctl(fd, VIDIOC_DQBUF, &buf)) {
                        switch (errno) {
                        case EAGAIN:
                                return 0;

                        case EIO:
                                /* Could ignore EIO, see spec. */

                                /* fall through */

                        default:
                                errno_exit("VIDIOC_DQBUF");
                        }
                }

                for (i = 0; i < n_buffers; ++i)
                        if (buf.m.userptr == (unsigned long)buffers[i].start
                            && buf.length == buffers[i].length)
                                break;

                assert(i < n_buffers);

                //process_image((void *)buf.m.userptr, buf.bytesused);

                if (-1 == xioctl(fd, VIDIOC_QBUF, &buf))
                        errno_exit("VIDIOC_QBUF");
                break;
        }

        return 1;
}

static void mainloop(void)
{
        unsigned int 	count;
        char			c;

        count = frame_count;

        while (count-- > 0) {
		fd_set fds;
		struct timeval tv;
		int r;

		FD_ZERO(&fds);
		FD_SET(fd, &fds);

		/* Timeout. */
		tv.tv_sec = 30;
		tv.tv_usec = 0;

		r = select(fd + 1, &fds, NULL, NULL, &tv);

		if (-1 == r) {
			if (EINTR == errno)
				continue;
			errno_exit("select");
		}

		if (0 == r) {
			fprintf(stderr, "select timeout\n");
			exit(EXIT_FAILURE);
		}

		read_frame();

                if(kbhit()) {
                	printf("\nhit char \n");
			c=getch(); /* consume the character */
			if(c=='q') return;
			if(c=='z')
				zoom_relative(fd,10);
			if(c=='c')
				zoom_relative(fd,-10);
			if(c=='a')
				pan_relative(fd,10);
			if(c=='d')
				pan_relative(fd,-10);
			if(c=='w')
				tilt_relative(fd,10);
			if(c=='x')
				tilt_relative(fd,-10);
			if(c=='+')
			{
				jpeg_quality=jpeg_quality+5;
				if(jpeg_quality>100) jpeg_quality=100;
				set_jpeg_quality(fd, jpeg_quality);
			}
			if(c=='-')
			{
				jpeg_quality=jpeg_quality-5;
				if(jpeg_quality<0) jpeg_quality=0;
				set_jpeg_quality(fd, jpeg_quality);
			}
		}
        }
}

static void stop_capturing(void)
{
        enum v4l2_buf_type type;

        switch (io) {
        case IO_METHOD_READ:
                /* Nothing to do. */
                break;

        case IO_METHOD_MMAP:
        case IO_METHOD_USERPTR:
                type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
                if (-1 == xioctl(fd, VIDIOC_STREAMOFF, &type))
                        errno_exit("VIDIOC_STREAMOFF");
                break;
        }
}

static void start_capturing(void)
{
        unsigned int i;
        enum v4l2_buf_type type;

        switch (io) {
        case IO_METHOD_READ:
       		printf("IO_METHOD_READ:Start capture\n");
                /* Nothing to do. */
                break;

        case IO_METHOD_MMAP:
      		printf("IO_METHOD_MMAP:Start capture nbuf=%d\n",n_buffers);
                for (i = 0; i < n_buffers; ++i) {
                        struct v4l2_buffer buf;

                        CLEAR(buf);
                        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
                        buf.memory = V4L2_MEMORY_MMAP;
                        buf.index = i;

                        if (-1 == xioctl(fd, VIDIOC_QBUF, &buf))
                                errno_exit("VIDIOC_QBUF");
                }
                type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
                if (-1 == xioctl(fd, VIDIOC_STREAMON, &type))
                        errno_exit("VIDIOC_STREAMON");
                break;

        case IO_METHOD_USERPTR:
                for (i = 0; i < n_buffers; ++i) {
                        struct v4l2_buffer buf;

                        CLEAR(buf);
                        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
                        buf.memory = V4L2_MEMORY_USERPTR;
                        buf.index = i;
                        buf.m.userptr = (unsigned long)buffers[i].start;
                        buf.length = buffers[i].length;

                        if (-1 == xioctl(fd, VIDIOC_QBUF, &buf))
                                errno_exit("VIDIOC_QBUF");
                }
                type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
                if (-1 == xioctl(fd, VIDIOC_STREAMON, &type))
                        errno_exit("VIDIOC_STREAMON");
                break;
        }
}

static void uninit_device(void)
{
        unsigned int i;

        switch (io) {
        case IO_METHOD_READ:
                free(buffers[0].start);
                break;

        case IO_METHOD_MMAP:
                for (i = 0; i < n_buffers; ++i)
                        if (-1 == munmap(buffers[i].start, buffers[i].length))
                                errno_exit("munmap");
                break;

        case IO_METHOD_USERPTR:
                for (i = 0; i < n_buffers; ++i)
                        free(buffers[i].start);
                break;
        }

        free(buffers);
}

static void init_read(unsigned int buffer_size)
{
        buffers = calloc(1, sizeof(*buffers));

        if (!buffers) {
                fprintf(stderr, "Out of memory\n");
                exit(EXIT_FAILURE);
        }

        buffers[0].length = buffer_size;
        buffers[0].start = malloc(buffer_size);

        if (!buffers[0].start) {
                fprintf(stderr, "Out of memory\n");
                exit(EXIT_FAILURE);
        }
}

static void init_mmap(void)
{
        struct v4l2_requestbuffers req;

        printf("\n\n ##### init_mmap ####\n");

        CLEAR(req);

        req.count = 4;
        req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        req.memory = V4L2_MEMORY_MMAP;

        if (-1 == xioctl(fd, VIDIOC_REQBUFS, &req)) {
                if (EINVAL == errno) {
                        fprintf(stderr, "%s does not support "
                                 "memory mapping\n", dev_name);
                        exit(EXIT_FAILURE);
                } else {
                        errno_exit("VIDIOC_REQBUFS");
                }
        }

        if (req.count < 2) {
                fprintf(stderr, "Insufficient buffer memory on %s\n",
                         dev_name);
                exit(EXIT_FAILURE);
        }

        buffers = calloc(req.count, sizeof(*buffers));

        printf("\ninit_mmap:buffer count %d pt=%p\n",req.count,buffers);

        if (!buffers) {
                fprintf(stderr, "Out of memory\n");
                exit(EXIT_FAILURE);
        }

        for (n_buffers = 0; n_buffers < req.count; ++n_buffers) {
                struct v4l2_buffer buf;

                CLEAR(buf);

                buf.type        = V4L2_BUF_TYPE_VIDEO_CAPTURE;
                buf.memory      = V4L2_MEMORY_MMAP;
                buf.index       = n_buffers;

                if (-1 == xioctl(fd, VIDIOC_QUERYBUF, &buf))
                        errno_exit("VIDIOC_QUERYBUF");

                printf("\ninit_mmap:buf[%d] len=%d offset=%d\n",n_buffers,buf.length,buf.m.offset);

                buffers[n_buffers].length = buf.length;
                buffers[n_buffers].start =
                        mmap(NULL /* start anywhere */,
                              buf.length,
                              PROT_READ | PROT_WRITE /* required */,
                              MAP_SHARED /* recommended */,
                              fd, buf.m.offset);

                if (MAP_FAILED == buffers[n_buffers].start)
                        errno_exit("mmap");
        }
}

static void init_userp(unsigned int buffer_size)
{
        struct v4l2_requestbuffers req;

        CLEAR(req);

        req.count  = 4;
        req.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        req.memory = V4L2_MEMORY_USERPTR;

        if (-1 == xioctl(fd, VIDIOC_REQBUFS, &req)) {
                if (EINVAL == errno) {
                        fprintf(stderr, "%s does not support "
                                 "user pointer i/o\n", dev_name);
                        exit(EXIT_FAILURE);
                } else {
                        errno_exit("VIDIOC_REQBUFS");
                }
        }

        buffers = calloc(4, sizeof(*buffers));

        if (!buffers) {
                fprintf(stderr, "Out of memory\n");
                exit(EXIT_FAILURE);
        }

        for (n_buffers = 0; n_buffers < 4; ++n_buffers) {
                buffers[n_buffers].length = buffer_size;
                buffers[n_buffers].start = malloc(buffer_size);

                if (!buffers[n_buffers].start) {
                        fprintf(stderr, "Out of memory\n");
                        exit(EXIT_FAILURE);
                }
        }
}

static void init_device(void)
{
        struct v4l2_capability cap;
        struct v4l2_cropcap cropcap;
        struct v4l2_crop crop;
        struct v4l2_format fmt;
        unsigned int min;

        printf("\n\n ##### init_device ####\n");

	//printf("\nSET FRAME RATE ******************");
	set_frame_rate(fd,ratio);

        if (-1 == xioctl(fd, VIDIOC_QUERYCAP, &cap)) {
                if (EINVAL == errno) {
                        fprintf(stderr, "%s is no V4L2 device\n",
                                 dev_name);
                        exit(EXIT_FAILURE);
                } else {
                        errno_exit("VIDIOC_QUERYCAP");
                }
        }

        if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
                fprintf(stderr, "%s is no video capture device\n",
                         dev_name);
                exit(EXIT_FAILURE);
        }

        switch (io) {
        case IO_METHOD_READ:
                if (!(cap.capabilities & V4L2_CAP_READWRITE)) {
                        fprintf(stderr, "%s does not support read i/o\n",
                                 dev_name);
                        exit(EXIT_FAILURE);
                }
                break;

        case IO_METHOD_MMAP:
        case IO_METHOD_USERPTR:
                if (!(cap.capabilities & V4L2_CAP_STREAMING)) {
                        fprintf(stderr, "%s does not support streaming i/o\n",
                                 dev_name);
                        exit(EXIT_FAILURE);
                }
                break;
        }


        /* Select video input, video standard and tune here. */


        CLEAR(cropcap);

        cropcap.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

        if (0 == xioctl(fd, VIDIOC_CROPCAP, &cropcap)) {
                crop.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
                crop.c = cropcap.defrect; /* reset to default */

                if (-1 == xioctl(fd, VIDIOC_S_CROP, &crop)) {
                        switch (errno) {
                        case EINVAL:
                                /* Cropping not supported. */
                                break;
                        default:
                                /* Errors ignored. */
                                break;
                        }
                }
        } else {
                /* Errors ignored. */
        }

        CLEAR(fmt);

	fmt.type 		= V4L2_BUF_TYPE_VIDEO_CAPTURE;
	fmt.fmt.pix.width       = img_width; //replace
	fmt.fmt.pix.height      = img_height; //replace
	if(img_jpeg)				      //
		fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_JPEG; //replace
	else
		fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV; //replace

	fmt.fmt.pix.field       = V4L2_FIELD_ANY;

	if (-1 == xioctl(fd, VIDIOC_S_FMT, &fmt))
		errno_exit("VIDIOC_S_FMT");

#ifdef notdef
        fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        if (force_format==1) {
                fmt.fmt.pix.width       = img_width; //replace
                fmt.fmt.pix.height      = img_height; //replace
                fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_JPEG; //replace
                fmt.fmt.pix.field       = V4L2_FIELD_ANY;

                if (-1 == xioctl(fd, VIDIOC_S_FMT, &fmt))
                        errno_exit("VIDIOC_S_FMT");

                /* Note VIDIOC_S_FMT may change width and height. */
	}
        else if (force_format==2) {
                fmt.fmt.pix.width       = img_width; //replace
                fmt.fmt.pix.height      = img_height; //replace
                fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_JPEG; //replace
                fmt.fmt.pix.field       = V4L2_FIELD_ANY;

                if (-1 == xioctl(fd, VIDIOC_S_FMT, &fmt))
                        errno_exit("VIDIOC_S_FMT");

                /* Note VIDIOC_S_FMT may change width and height. */



	} else {
                /* Preserve original settings as set by v4l2-ctl for example */
                if (-1 == xioctl(fd, VIDIOC_G_FMT, &fmt))
                        errno_exit("VIDIOC_G_FMT");
        }
#endif
        /* Buggy driver paranoia. */
        min = fmt.fmt.pix.width * 2;
        if (fmt.fmt.pix.bytesperline < min)
                fmt.fmt.pix.bytesperline = min;
        min = fmt.fmt.pix.bytesperline * fmt.fmt.pix.height;
        if (fmt.fmt.pix.sizeimage < min)
                fmt.fmt.pix.sizeimage = min;

        switch (io) {
        case IO_METHOD_READ:
                init_read(fmt.fmt.pix.sizeimage);
                break;

        case IO_METHOD_MMAP:
                init_mmap();
                break;

        case IO_METHOD_USERPTR:
                init_userp(fmt.fmt.pix.sizeimage);
                break;
        }
}

static void close_device(void)
{
        if (-1 == close(fd))
                errno_exit("close");

        fd = -1;
}

static void open_device(void)
{
        struct stat st;

        if (-1 == stat(dev_name, &st)) {
                fprintf(stderr, "Cannot identify '%s': %d, %s\n",
                         dev_name, errno, strerror(errno));
                exit(EXIT_FAILURE);
        }

        if (!S_ISCHR(st.st_mode)) {
                fprintf(stderr, "%s is no device\n", dev_name);
                exit(EXIT_FAILURE);
        }

        fd = open(dev_name, O_RDWR /* required */ | O_NONBLOCK, 0);

        if (-1 == fd) {
                fprintf(stderr, "Cannot open '%s': %d, %s\n",
                         dev_name, errno, strerror(errno));
                exit(EXIT_FAILURE);
        }
}

static void usage(FILE *fp, int argc, char **argv)
{
        fprintf(fp,
                "Usage: %s [options]\n\n"
                "Version 1.3\n"
                "Options:\n"
                "-d | --device name   Video device name [%s]\n"
		"-i | --ip_address    Remote IP address [%s]\n"
		"-z | --zoom          Zoom [%d]\n"
		"-s | --size          packet size [%d]\n"
		"-t | --ratio         Zoom [%d]\n"
                "-h | --help          Print this message\n"
                "-m | --mmap          Use memory mapped buffers [default]\n"
                "-r | --read          Use read() calls\n"
                "-u | --userp         Use application allocated buffers\n"
                "-o | --output        Outputs stream to stdout\n"
                "-f | --format        Force format to 640x480 YUYV\n"
                "-c | --count         Number of frames to grab [%i]\n"
                "",
                 argv[0], dev_name, addr, zoom, max_pkt_size, ratio, frame_count);
}

static const char short_options[] = "d:i:z:s:t:hmruof:c:";

static const struct option
long_options[] = {
        { "device", required_argument, NULL, 'd' },
        { "ip",     required_argument, NULL, 'i' },
        { "zoom",   required_argument, NULL, 'z' },
        { "size",   required_argument, NULL, 's' },
	{ "ratio",  required_argument, NULL, 't' },
        { "help",   no_argument,       NULL, 'h' },
        { "mmap",   no_argument,       NULL, 'm' },
        { "read",   no_argument,       NULL, 'r' },
        { "userp",  no_argument,       NULL, 'u' },
        { "output", no_argument,       NULL, 'o' },
        { "format", required_argument, NULL, 'f' },
        { "count",  required_argument, NULL, 'c' },
        { 0, 0, 0, 0 }
};



#define SA struct sockaddr

void tcp_open(void)
{
	//struct sockaddr_in servaddr;

	if ((sockfd = socket(AF_INET,SOCK_STREAM,0))==-1)
	{
		perror("\nOpen socket error");;
		exit(1);
	}

	bzero(&serv_addr,sizeof(serv_addr));
	serv_addr.sin_family=AF_INET;
	serv_addr.sin_addr.s_addr=htonl(INADDR_ANY);
	serv_addr.sin_port=htons(PORT);

	if(inet_pton(AF_INET,addr,&serv_addr.sin_addr)!=1)
	{
		perror("inet_pton error");
		exit(1);
	}

	if(connect(sockfd,(SA*)&serv_addr,sizeof(serv_addr)))
	{
		perror("\nconnect error");
		exit(1);
	}

}


void udp_open(void)
{
	//struct sockaddr_in	serv_addr;

	//setup UDP client:
	if ((sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP))==-1)
	{
		printf("Open socket error\n");;
		exit(1);
	}

        bzero(&serv_addr, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(PORT);
	if (inet_aton(addr, &serv_addr.sin_addr)==0)
	{
		printf("inet_aton error");
		exit(1);
	}
}

int main(int argc, char **argv)
{
        dev_name = "/dev/video0";
	addr = "0.0.0.0";
	zoom = 100;
	tcp_conn=0;
	ratio=1;
	max_pkt_size=MAX_PACKET_SIZE;
	img_jpeg=0;

	for (;;) {
                int idx;
                int c;

                c = getopt_long(argc, argv,
                                short_options, long_options, &idx);

                if (-1 == c)
                        break;

                switch (c) {
                case 0: /* getopt_long() flag */
                        break;

                case 'd':
                        dev_name = optarg;
                        break;
 		case 'i':
                        addr = optarg;
                        break;
		case 'z':
			zoom = strtol(optarg, NULL, 0);
                        break;
		case 't':
			ratio = strtol(optarg, NULL, 0);
                        break;
		case 's':
			max_pkt_size = strtol(optarg, NULL, 0);
                        break;

                case 'h':
                        usage(stdout, argc, argv);
                        exit(EXIT_SUCCESS);

                case 'm':
                        io = IO_METHOD_MMAP;
                        break;

                case 'r':
                        io = IO_METHOD_READ;
                        break;

                case 'u':
                        io = IO_METHOD_USERPTR;
                        break;

                case 'o':
                        out_buf++;
                        break;

                case 'f':
                        //force_format = strtol(optarg, NULL, 0);
			printf("\ntesting format:\n");
			if(strstr(optarg,"jpeg") != NULL) {
				printf("\njpeg_image\n");
				img_jpeg=1;
				image_type=TYP_JPEG;
			}
			else
				image_type=TYP_YUV422;

			if(strstr(optarg,"320x240") != NULL) {
				printf("\n320x240\n");
				img_height=240;
				img_width=320;
				image_format=FMT_320x240;
			}
			else if(strstr(optarg,"640x480") != NULL) {
				printf("\n640x480\n");
				img_height=480;
				img_width=640;
				image_format=FMT_640x480;
			}
			else if(strstr(optarg,"800x600") != NULL) {
				printf("\n800x600\n");
				img_height=600;
				img_width=800;
				image_format=FMT_800x600;
			}
                        break;

                case 'c':
                        errno = 0;
                        frame_count = strtol(optarg, NULL, 0);
                        if (errno)
                                errno_exit(optarg);
                        break;

                default:
                        usage(stderr, argc, argv);
                        exit(EXIT_FAILURE);
                }
        }

	if(tcp_conn)
		tcp_open();
	else
		udp_open();

 	printf("\ndevice name: [%s] ip=%s io=%x",dev_name,addr,io);

	open_device();
        init_device();
	zoom_absolute(fd,zoom);
        start_capturing();
        mainloop();
        stop_capturing();
        uninit_device();
        close_device();
        fprintf(stderr, "\n");
        return 0;
}
