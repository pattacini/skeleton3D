#ifndef SKELETON3D_H
#define SKELETON3D_H

#include <string>
#include <deque>
#include <map>
#include <ctime>
#include <stdio.h>
#include <unistd.h>

#include <yarp/os/all.h>
#include <yarp/sig/all.h>
#include <yarp/math/Math.h>
#include <yarp/math/Rand.h>
#include <iCub/ctrl/filters.h>

#include <icubclient/all.h>
#include "kinectWrapper/kinectWrapper.h"
#include "udp_client_server.h"
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>

#include <opencv2/opencv.hpp>

#include "skeleton3D_IDL.h"

//#include "iCub/vtMappingTF/vtMappingTF.h"

using namespace std;
using namespace yarp::os;
using namespace yarp::sig;
using namespace yarp::math;
using namespace iCub::ctrl;
using namespace icubclient;

class skeleton3D : public RFModule, public skeleton3D_IDL
{
protected:
    double      period;
    string      name;
    double      body_valence;
    double      hand_valence;
    double      part_dimension;
    bool        use_part_conf;
    bool        draw_lower;

    Stamp       ts;

    RpcServer   rpcPort;                            //!< rpc server to receive user request
    RpcClient   rpcGet3D;                           //!< rpc client port to send requests to SFM
    OPCClient   *opc;                               //!< OPC client object

    Port                    portToGui;
    yarp::os::Bottle        cmdGui;

    BufferedPort<Bottle>    bodyPartsInPort;        //!< buffered port of input of received body parts location in image
    BufferedPort<Bottle>    toolClassInPort;        //!< buffered port of input of received class of tool from onTheFlyRecoginition
    BufferedPort<Bottle>    toolClassInPort_left;   //!< buffered port of input of received class of tool from onTheFlyRecoginition

    BufferedPort<Bottle>    ppsOutPort;             //!< buffered port of output to send body parts as obstacles to PPS (visuoTactileWrapper)
    BufferedPort<Bottle>    handBlobPort;           //!< buffered port of output to send blob with @see hand_with_tool
    BufferedPort<Bottle>    handBlobPort_left;      //!< buffered port of output to send blob with @see hand_with_tool
    RpcClient               rpcAskTool;             //!< rpc client port to send requests to /onTheFlyRecognition/human:io

    Mutex                   mutexResourcesSkeleton;
    Mutex                   mutexResourcesSFM;
    Mutex                   mutexResourcesTool;

    bool                    connected3D;
    bool                    connectedPPS;

    string                  partner_default_name;   //!< string value of default name of partner
    icubclient::Agent*      partner;                //!< human as an agent object
    kinectWrapper::Player   player;
    map<string,double>      confJoints;             //!< confidence of identified skeleton
    double                  partConfThres;          //!< threshold value of identified confidence
    double                  workspaceX, workspaceY; //!< threshold value for 3D skeleton, ignore skeleton outside this range
    double                  workspaceX_min;
    double                  workspaceY_max;

    double                  dSince;                 //!< double value of timers
    unsigned long           dTimingLastApparition;  //!< time struct of the last appartition of an agent
    double                  dThresholdDisparition;  //!< timing maximal of non-reconnaissance of a agent, after thath we consider the agent as absent

    bool                        use_part_filter;    //!< boolean value to define the usage of median filter for body parts
    bool                        init_filters;       //!< boolean value to define if the median filter for body parts have been initialized
    bool                        init_filters_angle; //!< boolean value to define if the median filter for body angles have been initialized
    int                         filterOrder;        //!< integer value for order of the median filter of the body parts
    map<string,MedianFilter>    filterSkeleton;     //!< median filter for position of a skeleton
    MedianFilter                *filterAngles;      //!< median filter for angle of the skeleton
    int                         filterOrderAngles;  //!< integer value for order of the median filter of the body angles

    bool                        use_fake_hand;
    yarp::sig::Vector           fakeHandPos;        //!< position of fake hand

    bool                        use_mid_arms;       //!< flag for calculation of midpoints in forearms

    double                      segLMax, segLMin;   //!< threshold for arm constraint

    int                                 sock, sockTool;
    sockaddr_in                         serveraddr, serveraddrTool;

    double                              radius;     //!< hand blob radius
    string                              hand_with_tool;
    CvPoint                             handCV, handCV_right, handCV_left, elbowCV_left, elbowCV_right;
    float                               tool_code[3];
    double                              tool_timer;
    unsigned long                       tool_lastClock;
    string                              toolLabelR;
    string                              toolLabelL;
    bool                                hasToolR, hasToolL;
    bool                                tool_training;

    unsigned int                        counterToolL, counterToolR;
    unsigned int                        counterHand, counterDrill, counterPolisher;
    float                               sendData49, sendData51;

    void    filt(map<string,kinectWrapper::Joint> &joints, map<string,kinectWrapper::Joint> &jointsFiltered);

    void    filtAngles(const Vector &angles, Vector &filteredAngles);

    /**
     * @brief get3DPosition Get the 3D point coordinate in Root frame through SFM
     * @param point A CvPoint for the estimated coordinate of an image pixel.
     * @param x Vector for 3D point coordinate. If SFM return more than one 3D point as result, the coordinate is average of all result
     * @return True if can get 3D point from SFM, False otherwise
     */
    bool    get3DPosition(const CvPoint &point, Vector &x);

    /**
     * @brief obtainBodyParts Get body parts position and confidence from Tensorflow-based skeleton2D module
     * @param partsCV Set of CvPoint(s) of body parts from skeleton2D module
     * @return True if can obtain the good value from skeleton2D module, False otherwise
     */
    bool    obtainBodyParts(deque<CvPoint> &partsCV);

    /**
     * @brief addJoint Add a joint of skeleton by name to a skeleton
     * @param joints A skeleton by set of joints and equivalent joint name
     * @param point CvPoint of the body part from skeleton2D module
     * @param partName String value of the name of a part
     */
    void    addJoint(map<string,kinectWrapper::Joint> &joints, const CvPoint &point, const string &partName);

    /**
     * @brief addConf Store the received identified confidence rate of a body part
     * @param conf Double value of the confidence rate
     * @param partName String value of the name of a part
     */
    void    addConf(const double &conf, const string &partName);

    bool    getPartPose(Agent* a, const string &partName, Vector &pose);

    void    addPartToStream(const Vector &pose, const string &partName, Bottle &streamedObjs);

    void    addMidArmsToStream(Bottle &streamedObjs);

    /**
     * @brief streamPartsToPPS Send body parts to (PPS) visuoTactileWrapper
     * @return True if send successfully, False otherwise
     */
    bool    streamPartsToPPS();

    /**
     * @brief addPartToStream Accumulate a body part by name to streamed Bottle before sending it to PPS
     * @param a An icubclient Agent, which contains all OPC-related information, e.g skeleton, position, dimensions of parts, valence of parts
     * @param partName String value of the name of a part
     * @param streamedObj A Yarp Bottle to store streamed body parts
     */
    void    addPartToStream(Agent* a, const string &partName, Bottle &streamedObj);

    /**
     * @brief computeValence Compute the body part valence based on the identified confidence rate
     * @param partName String for the name of a body part
     * @return Double value of the body part valence
     */
    double  computeValence(const string &partName);

    void    computeSpine(map<string,kinectWrapper::Joint> &jnts);

    /**
     * @brief extrapolateHand Estimate the hand position, known the elbow and wrist position
     * @param jnts A skeleton by set of joints and equivalent joint name
     */
    void    extrapolateHand(map<string,kinectWrapper::Joint> &jnts);

    /**
     * @brief extrapolatePoint Estimate a point position, known 2 other points in the same line
     * @param p1 Yarp Vector of 1st point in the line
     * @param p2 Yarp Vector of 2nd point in the line
     * @param result Estimated position
     * @return True if can estimate the point, False otherwise and size of p1 and/or p2 is wrong
     */
    bool    extrapolatePoint(const Vector &p1, const Vector &p2, Vector &result);

    bool    constraintLink(const Vector &p1, const Vector &p2, Vector &result,
                           const double &segLMin, const double &segLMax, const double &segLNormal);

    void    constraintBodyLinks(map<string, kinectWrapper::Joint> &jnts);

    void    constraintOneBodyLink(map<string, kinectWrapper::Joint> &jnts,
                                  const string &partName1, const string &partName2,
                                  const double &segLMin, const double &segLMax, const double &segLNormal);

    void    constraintHip(map<string, kinectWrapper::Joint> &jnts);


    void    initShowBodySegGui(const string &segmentName, const string &color);

    void    updateBodySegGui(const vector<Vector> &segment, const string &segmentName);

    void    deleteBodySegGui(const string &segmentName);

    bool    drawBodyGui(icubclient::Agent *a);

    bool    assignJointByVec(kinectWrapper::Joint &jnt, const Vector &pos);

    void    addJointAndConf(map<string,kinectWrapper::Joint> &joints,
                            const Vector &pos, const string &partName);

    double  angleAtJoint(const Vector &v1, const Vector &v2, const double &direction);
    double  angleAtJointTan(const Vector &v1, const Vector &v2, const double &direction);
    double  angleAtJointTan(const Vector &v1, const Vector &v2, const Vector &direction);
    Vector  vectorBetweenJnts(const Vector &jnt1, const Vector &jnt2);
    Vector  computeAllBodyAngles();

    /**
     * @brief computeBodyAngle compute the angle (deg) between 2 sucessive body links
     * @param partName1 name of joint 1, where angle need calculated
     * @param partName2 name of joint 2
     * @param partName3 name of joint 3
     * @return angle at joint 1 between 2 links, i.e link12 (joint1-joint2) and link13 (joint1-joint3)
     */
    double  computeBodyAngle(const string &partName1,
                             const string &partName2,
                             const string &partName3,
                             const double &direction);
    double  computeBodyAngle(const Vector &jnt1,
                             const Vector &jnt2,
                             const Vector &jnt3,
                             const double &direction);

    double  computeFootAngle(const string &partName1,
                             const string &partName2,
                             const double &direction);

    void    computeLowerBodyMeanAngles(double &angleHip,
                                       double &angleKnee,
                                       double &angleAnkle);

    void    constraintNegAngle(double &angle, const double &maxValue);

    Vector  joint2Vector(const kinectWrapper::Joint &joint);

    Vector  computeAdaptiveBlobCoffs(const Vector &x1, const Vector &x2);
    bool    cropHandBlob(const string &hand, Vector &blob);
    bool    askToolLabel(string &label);

    bool    toolRecognition(const string &hand, string &toolLabel);

    bool    configure(ResourceFinder &rf);
    bool    interruptModule();
    bool    close();
    bool    attach(RpcServer &source);
    double  getPeriod();
    bool    updateModule();
public:

    /************************************************************************/
    // Thrift methods
    bool set_valence(const double _valence)
    {
        if (_valence<=1.0 && _valence>=-1.0)
        {
            body_valence = _valence;
            hand_valence = _valence;
            return true;
        }
        else
            return false;
    }

    double get_valence()
    {
        return body_valence;
    }

    bool set_valence_hand(const double _valence)
    {
        if (_valence<=1.0 && _valence>=-1.0)
        {
            hand_valence = _valence;
            return true;
        }
        else
            return false;
    }

    double get_valence_hand()
    {
        return hand_valence;
    }

    bool set_filter_order(const int16_t _order)
    {
        if (_order>=0)
        {
            filterOrder = _order;
            return true;
        }
        else
            return false;
    }

    int16_t get_filter_order()
    {
        return filterOrder;
    }

    bool set_threshold_disparition(const double _thr)
    {
        if (_thr>0.0)
        {
            dThresholdDisparition = _thr;
            return true;
        }
        else
            return false;
    }

    double get_threshold_disparition()
    {
        return dThresholdDisparition;
    }

    bool set_fake_hand_pos(const yarp::sig::Vector& _pos)
    {
        if (fakeHandPos.size() == _pos.size())
        {
            fakeHandPos = _pos;
            return true;
        }
        else
            return false;
    }

    yarp::sig::Vector get_fake_hand_pos()
    {
        return fakeHandPos;
    }

    bool enable_fake_hand()
    {
        use_fake_hand = true;
        return true;
    }

    bool disable_fake_hand()
    {
        use_fake_hand = false;
        return true;
    }

    bool enable_part_conf()
    {
        use_part_conf = true;
        return true;
    }

    bool disable_part_conf()
    {
        use_part_conf = false;
        return true;
    }

    bool enable_mid_arms()
    {
        use_mid_arms = true;
        return true;
    }

    bool disable_mid_arms()
    {
        use_mid_arms = false;
        return true;
    }

    bool enable_tool_training(const string &hand)
    {
        tool_training = true;
        hand_with_tool = hand;
        yInfo("hand_with_tool is %s",hand_with_tool.c_str());
        return true;
    }

    bool disable_tool_training()
    {
        tool_training = false;
        return true;
    }

    bool set_workspace_x(const double workspace_x)
    {
        if (workspace_x>=0)
        {
            workspaceX = workspace_x;
            return true;
        }
        else
            return false;
    }

    double get_workspace_x()
    {
        return workspaceX;
    }

    bool set_workspace_x_min(const double workspace_x_min)
    {
        workspaceX_min = workspace_x_min;
        return true;
    }

    double get_workspace_x_min()
    {
        return workspaceX_min;
    }

    bool set_workspace_y(const double workspace_y)
    {
        if (workspace_y>=0)
        {
            workspaceY = workspace_y;
            return true;
        }
        else
            return false;
    }

    double get_workspace_y()
    {
        return workspaceY;
    }

    bool set_workspace_y_max(const double workspace_y_max)
    {
        if (workspace_y_max>=0)
        {
            workspaceY_max = workspace_y_max;
            return true;
        }
        else
            return false;
    }

    double get_workspace_y_max()
    {
        return workspaceY_max;
    }


    std::map<unsigned int, std::string> mapPartsOpenPose {
        {0,  "Nose"},
        {1,  "Neck"},
        {2,  "RShoulder"},
        {3,  "RElbow"},
        {4,  "RWrist"},
        {5,  "LShoulder"},
        {6,  "LElbow"},
        {7,  "LWrist"},
        {8,  "RHip"},
        {9,  "RKnee"},
        {10, "RAnkle"},
        {11, "LHip"},
        {12, "LKnee"},
        {13, "LAnkle"},
        {14, "REye"},
        {15, "LEye"},
        {16, "REar"},
        {17, "LEar"},
        {18, "Background"}
    };
    std::map<unsigned int, std::string> mapPartsKinect {
        {0,  "head"},
        {1,  "shoulderCenter"},
        {2,  "shoulderRight"},
        {3,  "elbowRight"},
        {4,  "handRight"},      // wrist --> hand: use extrapolatePoint later to convert truely
        {5,  "shoulderLeft"},
        {6,  "elbowLeft"},
        {7,  "handLeft"},       // wrist --> hand: use extrapolatePoint later to convert truely
        {8,  "hipRight"},
        {9,  "kneeRight"},
        {10, "ankleRight"},
        {11, "hipLeft"},
        {12, "kneeLeft"},
        {13, "ankleLeft"},
    };

    std::map<unsigned int, std::string> mapPartsGui {
        {0,  "handLeft"},
        {1,  "elbowLeft"},
        {2,  "shoulderLeft"},
        {3,  "shoulderCenter"},
        {4,  "shoulderRight"},
        {5,  "elbowRight"},
        {6,  "handRight"},      // wrist --> hand: use extrapolatePoint later to convert truely
        {7,  "ankleLeft"},
        {8,  "kneeLeft"},
        {9,  "hipLeft"},
        {10, "hipRight"},
        {11, "kneeRight"},
        {12, "ankleRight"},
        {13, "head"},
    };

    std::map<unsigned int, std::string> mapPartsUdp {
        {0,  "hipLeft"},
        {1,  "kneeLeft"},
        {2,  "ankleLeft"},
        {3,  "hipRight"},
        {4,  "kneeRight"},
        {5,  "ankleRight"},
        {6,  "shoulderLeft"},
        {7,  "elbowLeft"},
        {8,  "handLeft"},
        {9,  "shoulderRight"},
        {10, "elbowRight"},
        {11, "handRight"},      // wrist --> hand: use extrapolatePoint later to convert truely
        {12, "head"},

    };
};

#endif // SKELETON3D_H
