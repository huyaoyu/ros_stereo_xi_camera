#include "ROSNode/SXCSync.hpp"

using namespace cv;
using namespace SRN;

SXCSync::SXCSync(const std::string& name)
: SyncROSNode(name),
  CAM_0_IDX(0), CAM_1_IDX(1),
  mTopicNameLeftImage("left/image_raw"), mTopicNameRightImage("right/image_raw"), 
  mOutDir("./"),
  mPrepared(false),
  mImageTransport(NULL), mPublishersImage(NULL), 
  mStereoXiCamera(NULL),
  mMbAEAG(NULL),
  mCvImages(NULL),
  mCP(NULL),
  mNImages(0),
  mROSLoopRate(NULL),
  mAutoGainExposurePriority(DEFAULT_AUTO_GAIN_EXPOSURE_PRIORITY),
  mAutoGainExposureTargetLevel(DEFAULT_AUTO_GAIN_EXPOSURE_TARGET_LEVEL),
  mAutoExposureTopLimit(DEFAULT_AUTO_EXPOSURE_TOP_LIMIT),
  mAutoGainTopLimit(DEFAULT_AUTO_GAIN_TOP_LIMIT),
  mTotalBandwidth(DEFAULT_TOTAL_BANDWIDTH),
  mBandwidthMargin(DEFAULT_BANDWIDTH_MARGIN),
  mFlagWriteImage(0),
  mLoopRate(DEFAULT_LOOP_RATE),
  mExternalTrigger(0),
  mNextImageTimeout_ms(DEFAULT_NEXT_IMAGE_TIMEOUT_MS),
  mSelfAdjust(1),
  mCustomAEAGEnabled(0),
  mCustomAEAGPriority(DEFAULT_CUSTOM_AEAG_PRIORITY),
  mCustomAEAGExposureTopLimit(DEFAULT_CUSTOM_AEAG_EXPOSURE_TOP_LIMIT),
  mCustomAEAGGainTopLimit(DEFAULT_CUSTOM_AEAG_GAIN_TOP_LIMIT),
  mCustomAEAGBrightnessLevel(DEFAULT_CUSTOM_AEAG_BRIGHTNESS_LEVEL),
  mVerbose(DEFAULT_VERBOSE)
{
    mXiCameraSN[CAM_0_IDX] = "CUCAU1814018";
    mXiCameraSN[CAM_1_IDX] = "CUCAU1814020";
}

SXCSync::~SXCSync()
{
    destroy_members();
}

Res_t SXCSync::parse_launch_parameters(void)
{
    // ============ Get the parameters from the launch file. ============== 
	
    std::string pXICameraSN_0 = mXiCameraSN[CAM_0_IDX];
	std::string pXICameraSN_1 = mXiCameraSN[CAM_1_IDX];

	ROSLAUNCH_GET_PARAM((*mpROSNode), "pAutoGainExposurePriority", mAutoGainExposurePriority, DEFAULT_AUTO_GAIN_EXPOSURE_PRIORITY);
	ROSLAUNCH_GET_PARAM((*mpROSNode), "pAutoGainExposureTargetLevel", mAutoGainExposureTargetLevel, DEFAULT_AUTO_GAIN_EXPOSURE_TARGET_LEVEL);
	ROSLAUNCH_GET_PARAM((*mpROSNode), "pAutoExposureTopLimit", mAutoExposureTopLimit, DEFAULT_AUTO_EXPOSURE_TOP_LIMIT);
	ROSLAUNCH_GET_PARAM((*mpROSNode), "pAutoGainTopLimit", mAutoGainTopLimit, DEFAULT_AUTO_GAIN_TOP_LIMIT);
	ROSLAUNCH_GET_PARAM((*mpROSNode), "pTotalBandwidth", mTotalBandwidth, DEFAULT_TOTAL_BANDWIDTH);
	ROSLAUNCH_GET_PARAM((*mpROSNode), "pBandwidthMargin", mBandwidthMargin, DEFAULT_BANDWIDTH_MARGIN);
	ROSLAUNCH_GET_PARAM((*mpROSNode), "pLoopRate", mLoopRate, DEFAULT_LOOP_RATE);
	ROSLAUNCH_GET_PARAM((*mpROSNode), "pFlagWriteImage", mFlagWriteImage, 0);
	ROSLAUNCH_GET_PARAM((*mpROSNode), "pOutDir", mOutDir, "./");
	ROSLAUNCH_GET_PARAM((*mpROSNode), "pExternalTrigger", mExternalTrigger, 0);
	ROSLAUNCH_GET_PARAM((*mpROSNode), "pNextImageTimeout_ms", mNextImageTimeout_ms, DEFAULT_NEXT_IMAGE_TIMEOUT_MS);
	ROSLAUNCH_GET_PARAM((*mpROSNode), "pSelfAdjust", mSelfAdjust, 1)
	ROSLAUNCH_GET_PARAM((*mpROSNode), "pCustomAEAGEnabled", mCustomAEAGEnabled, 0);
	ROSLAUNCH_GET_PARAM((*mpROSNode), "pCustomAEAGPriority", mCustomAEAGPriority, DEFAULT_CUSTOM_AEAG_PRIORITY);
	ROSLAUNCH_GET_PARAM((*mpROSNode), "pCustomAEAGExposureTopLimit", mCustomAEAGExposureTopLimit, DEFAULT_CUSTOM_AEAG_EXPOSURE_TOP_LIMIT);
	ROSLAUNCH_GET_PARAM((*mpROSNode), "pCustomAEAGGainTopLimit", mCustomAEAGGainTopLimit, DEFAULT_CUSTOM_AEAG_GAIN_TOP_LIMIT);
	ROSLAUNCH_GET_PARAM((*mpROSNode), "pCustomAEAGBrightnessLevel", mCustomAEAGBrightnessLevel, DEFAULT_CUSTOM_AEAG_BRIGHTNESS_LEVEL);
	ROSLAUNCH_GET_PARAM((*mpROSNode), "pXICameraSN_0", pXICameraSN_0, mXiCameraSN[CAM_0_IDX]);
	ROSLAUNCH_GET_PARAM((*mpROSNode), "pXICameraSN_1", pXICameraSN_1, mXiCameraSN[CAM_1_IDX]);

    mXiCameraSN[CAM_0_IDX] = pXICameraSN_0;
    mXiCameraSN[CAM_1_IDX] = pXICameraSN_1;

    return RES_OK;
}

Res_t SXCSync::prepare(void)
{
    Res_t res = RES_OK;

    if ( false == mPrepared )
    {
        // =================== Image publishers. ===================
        mImageTransport = new image_transport::ImageTransport((*mpROSNode));

        mPublishersImage = new image_transport::Publisher[2];
        mPublishersImage[CAM_0_IDX] = mImageTransport->advertise(mTopicNameLeftImage,  1);
        mPublishersImage[CAM_1_IDX] = mImageTransport->advertise(mTopicNameRightImage, 1);

        // Stereo camera object.
        mStereoXiCamera = new sxc::StereoXiCamera(mXiCameraSN[CAM_0_IDX], mXiCameraSN[CAM_1_IDX]);

        // Trigger selection.
        if ( 1 == mExternalTrigger )
        {
            mStereoXiCamera->enable_external_trigger(mNextImageTimeout_ms);
        }

        // Custom AEAG.
        if ( 1 == mCustomAEAGEnabled )
        {
            mMbAEAG = new sxc::MeanBrightness;
            mMbAEAG->set_exposure_top_limit(mCustomAEAGExposureTopLimit);
            mMbAEAG->set_gain_top_limit(sxc::dBToGain(mCustomAEAGGainTopLimit));
            mMbAEAG->set_priority(mCustomAEAGPriority);

            mStereoXiCamera->set_custom_AEAG(mMbAEAG);
            mStereoXiCamera->set_custom_AEAG_target_brightness_level(mCustomAEAGBrightnessLevel);
            mStereoXiCamera->enable_custom_AEAG();
        }

        // Prepare with the camera API.
        try
        {
            // Configure the stereo camera.
            mStereoXiCamera->set_autogain_exposure_priority(mAutoGainExposurePriority);
            mStereoXiCamera->set_autogain_exposure_target_level(mAutoGainExposureTargetLevel);
            mStereoXiCamera->set_autoexposure_top_limit(mAutoExposureTopLimit);
            mStereoXiCamera->set_autogain_top_limit(mAutoGainTopLimit);
            mStereoXiCamera->set_total_bandwidth(mTotalBandwidth);
            mStereoXiCamera->set_bandwidth_margin(mBandwidthMargin);

            // Pre-open, open and configure the stereo camera.
            mStereoXiCamera->open();

            // Self-adjust.
            if ( 1 == mSelfAdjust )
            {
                ROS_INFO("Perform self-adjust...");
                mStereoXiCamera->self_adjust(true);
                ROS_INFO("Self-adjust done.");
            }

            // Get the sensor array.
            std::string strSensorArray;
            mStereoXiCamera->put_sensor_filter_array(0, strSensorArray);
            ROS_INFO("The sensor array string is %s.", strSensorArray.c_str());

            mCvImages = new Mat[2];
            mCP = new sxc::StereoXiCamera::CameraParams_t[2];

            mJpegParams.push_back( CV_IMWRITE_JPEG_QUALITY );
            mJpegParams.push_back( 100 );

            mNImages = 0;

            // Running rate.
            mROSLoopRate = new ros::Rate(mLoopRate);

            mPrepared = true;
        }
        catch  ( boost::exception &ex )
        {
            ROS_ERROR("Exception catched.");
            if ( std::string const * expInfoString = boost::get_error_info<sxc::ExceptionInfoString>(ex) )
            {
                ROS_ERROR("%s", expInfoString->c_str());
            }

            std::string diagInfo = boost::diagnostic_information(ex, true);

            ROS_ERROR("%s", diagInfo.c_str());

            res = RES_ERROR;
        }

        return res;
    }
    else
    {
        std::cout << "Error: Already prepared." << std::endl;
        return RES_ERROR;
    }
}

Res_t SXCSync::resume(void)
{
    // Start acquisition.
    ROS_INFO("%s", "Start acquisition.");
    mStereoXiCamera->start_acquisition();

    return RES_OK;
}

Res_t SXCSync::synchronize(void)
{
    Res_t res = RES_OK;

    int getImagesRes = 0;
    std::stringstream ss;   // String stream for outputing info.

    try
    {
        ROS_INFO("nImages = %d", mNImages);

        // Trigger.
        if ( false == mStereoXiCamera->is_external_triger() )
        {
            mStereoXiCamera->software_trigger();
        }

        // Get images.
        getImagesRes = mStereoXiCamera->get_images( mCvImages[0], mCvImages[1], mCP[0], mCP[1] );

        if ( 0 != getImagesRes )
        {
            ROS_ERROR("Get images failed");
        }
        else
        {
            // Prepare the time stamp for the header.
            mRosTimeStamp = ros::Time::now();

            // Convert the image into ROS image message.
            LOOP_CAMERAS_BEGIN
                // Clear the temporary sting stream.
                ss.flush();	ss.str(""); ss.clear();
                ss << mOutDir << "/" << mNImages << "_" << loopIdx;

                if ( true == mVerbose )
                {
                    ROS_INFO( "%s", ss.str().c_str() );
                }

                // Save the captured image to file system.
                if ( 1 == mFlagWriteImage )
                {
                    // std::string yamlFilename = ss.str() + ".yaml";
                    std::string imgFilename = ss.str() + ".bmp";

                    // FileStorage cfFS(yamlFilename, FileStorage::WRITE);
                    // cfFS << "frame" << nImages << "image_id" << loopIdx << "raw_data" << cvImages[loopIdx];
                    imwrite(imgFilename, mCvImages[loopIdx], mJpegParams);
                }
                
                ROS_INFO( "Camera %d captured image (%d, %d). AEAG %d, AEAGP %.2f, exp %.3f ms, gain %.1f dB.", 
                        loopIdx, mCvImages[loopIdx].rows, mCvImages[loopIdx].cols,
                        mCP[loopIdx].AEAGEnabled, mCP[loopIdx].AEAGPriority, mCP[loopIdx].exposure / 1000.0, mCP[loopIdx].gain );

                // Publish images.
                mMsgImage = cv_bridge::CvImage(std_msgs::Header(), "bgr8", mCvImages[loopIdx]).toImageMsg();

                mMsgImage->header.seq   = mNImages;
                mMsgImage->header.stamp = mRosTimeStamp;

                mPublishersImage[loopIdx].publish(mMsgImage);

                if ( true == mVerbose )
                {
                    ROS_INFO("%s", "Message published.");
                }
            LOOP_CAMERAS_END
        }

        // ROS spin.
        ros::spinOnce();

        // Sleep.
        mROSLoopRate->sleep();

        mNImages++;
    }
    catch ( boost::exception &ex )
    {
        ROS_ERROR("Exception catched.");
        if ( std::string const * expInfoString = boost::get_error_info<sxc::ExceptionInfoString>(ex) )
        {
            ROS_ERROR("%s", expInfoString->c_str());
        }

        std::string diagInfo = boost::diagnostic_information(ex, true);

        ROS_ERROR("%s", diagInfo.c_str());

        res = RES_ERROR;
    }

    return res;
}

Res_t SXCSync::pause(void)
{
    // Stop acquisition.
    ROS_INFO("Stop acquisition.");
    mStereoXiCamera->stop_acquisition();

    return RES_OK;
}

Res_t SXCSync::destroy(void)
{
    // Close.
    mStereoXiCamera->close();
    ROS_INFO("Stereo camera closed.");

    destroy_members();

    mPrepared = false;

    return RES_OK;
}

void SXCSync::destroy_members(void)
{
    if ( NULL != mROSLoopRate )
    {
        delete mROSLoopRate; mROSLoopRate = NULL;
    }

    if ( NULL != mCP )
    {
        delete [] mCP; mCP = NULL;
    }

    if ( NULL != mCvImages )
    {
        delete [] mCvImages; mCvImages = NULL;
    }

    if ( NULL != mMbAEAG )
    {
        delete mMbAEAG; mMbAEAG = NULL;
    }

    if ( NULL != mStereoXiCamera )
    {
        delete mStereoXiCamera; mStereoXiCamera = NULL;
    }

    if ( NULL != mPublishersImage )
    {
        delete [] mPublishersImage; mPublishersImage = NULL;
    }

    if ( NULL != mImageTransport )
    {
        delete mImageTransport; mImageTransport = NULL;
    }
}

void SXCSync::set_topic_name_left_image(const std::string& name)
{
    mTopicNameLeftImage = name;
}

const std::string& SXCSync::get_topi_name_left_image(void)
{
    return mTopicNameLeftImage;
}

void SXCSync::set_topic_name_right_image(const std::string& name)
{
    mTopicNameRightImage = name;
}

const std::string& SXCSync::get_topi_name_right_image(void)
{
    return mTopicNameRightImage;
}

void SXCSync::set_out_dir(const std::string& outDir)
{
    mOutDir = outDir;
}

const std::string& SXCSync::get_out_dir(void)
{
    return mOutDir;
}