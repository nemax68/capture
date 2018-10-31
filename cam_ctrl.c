#include <stdio.h>
#include <fcntl.h>              /* low-level i/o */
#include <errno.h>

#include <sys/ioctl.h>
#include <linux/videodev2.h>

int xioctl(int fh, int request, void *arg);

int zoom_absolute(int fd, int zoom);
int zoom_relative(int fd, int zoom);
int pan_relative(int fd, int pan);
int pan_absolute(int fd, int pan);
int tilt_relative(int fd, int tilt);
int tilt_absolute(int fd, int tilt);

int xioctl(int fh, int request, void *arg)
{
	int r;

	do {
		r = ioctl(fh, request, arg);
	} while (-1 == r && EINTR == errno);

	return r;
}

int set_jpeg_quality(int fd, int quality)
{
	struct v4l2_control ctrl;

	ctrl.id=V4L2_CID_JPEG_COMPRESSION_QUALITY;
	ctrl.value=quality;

	return(xioctl(fd, VIDIOC_S_CTRL, &ctrl));
}

int set_frame_rate(int fd, int rate)
{
	struct v4l2_streamparm parm;

	parm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

	parm.parm.capture.timeperframe.numerator = rate;
	parm.parm.capture.timeperframe.denominator = 1;

	int ret = xioctl(fd, VIDIOC_S_PARM, &parm);

	if (ret < 0)
		return 0;

	return 1;
}

int zoom_absolute(int fd, int zoom)
{
	struct v4l2_control ctrl;

	//printf("\n###V4L2_CID_ZOOM_ABSOLUTE###");

	ctrl.id=V4L2_CID_ZOOM_ABSOLUTE;
	ctrl.value=zoom;

	return(xioctl(fd, VIDIOC_S_CTRL, &ctrl));
}

/*
 * zoom positive = in
 * zoom negative = out
 *
 */

int zoom_relative(int fd, int zoom)
{
	struct v4l2_control ctrl;

	//printf("Frame %d zoom in\n",frame_number);

	ctrl.id=V4L2_CID_ZOOM_RELATIVE;
	ctrl.value=zoom;

	return(xioctl(fd, VIDIOC_S_CTRL, &ctrl));
}

/*
 * pan positive = left
 * pan negative = right
 *
 */

int pan_relative(int fd, int pan)
{
	struct v4l2_control ctrl;

	//printf("Frame %d pan left\n",frame_number);

	ctrl.id=V4L2_CID_PAN_RELATIVE;
	ctrl.value=pan;

	return(xioctl(fd, VIDIOC_S_CTRL, &ctrl));
}

int pan_absolute(int fd, int pan)
{
	struct v4l2_control ctrl;

	//printf("Frame %d pan right\n",frame_number);

	ctrl.id=V4L2_CID_PAN_ABSOLUTE;
	ctrl.value=pan;

	return(xioctl(fd, VIDIOC_S_CTRL, &ctrl));
}

int tilt_relative(int fd, int tilt)
{
	struct v4l2_control ctrl;

	//printf("Frame %d tilt up\n",frame_number);

	ctrl.id=V4L2_CID_TILT_RELATIVE;
	ctrl.value=tilt;

	return(xioctl(fd, VIDIOC_S_CTRL, &ctrl));
}

int tilt_absolute(int fd, int tilt)
{
	struct v4l2_control ctrl;

	//printf("Frame %d tilt down\n",frame_number);

	ctrl.id=V4L2_CID_TILT_RELATIVE;
	ctrl.value=tilt;

	return(xioctl(fd, VIDIOC_S_CTRL, &ctrl));
}

