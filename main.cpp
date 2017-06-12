#include <stdio.h>
#include <iostream>
#include <opencv2/opencv.hpp>
#include <opencv2/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/video/video.hpp>
#include <opencv2/objdetect/objdetect.hpp>
#include <ctime>
#include <omp.h>
#include <map>
#include "base64.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include<string>  //string
#include<sys/socket.h>    //socket
#include<arpa/inet.h> //inet_addr
#include<netdb.h> //hostent

#include <boost/thread.hpp>
#include <boost/chrono.hpp>

#define DITHERING_DIVISION_FACTOR 64
using namespace std;



class tcp_client
{
private:
    int sock;
    std::string address;
    int port;
    struct sockaddr_in server;

public:
    tcp_client();
    bool conn(string, int);
    bool send_data(string data);
    string receive(int);
};

tcp_client::tcp_client()
{
    sock = -1;
    port = 0;
        address = "";
}

/**
    Connect to a host on a certain port number
*/
bool tcp_client::conn(string address , int port)
{
    //create socket if it is not already created
    if(sock == -1)
    {
        //Create socket
        sock = socket(AF_INET , SOCK_STREAM , 0);
        if (sock == -1)
        {
            perror("Could not create socket");
        }

        cout<<"Socket created\n";
    }
    else    {   /* OK , nothing */  }

    //setup address structure
    if(inet_addr(address.c_str()) == -1)
    {
        struct hostent *he;
        struct in_addr **addr_list;

        //resolve the hostname, its not an ip address
        if ( (he = gethostbyname( address.c_str() ) ) == NULL)
        {
            //gethostbyname failed
            herror("gethostbyname");
            cout<<"Failed to resolve hostname\n";

            return false;
        }

        //Cast the h_addr_list to in_addr , since h_addr_list also has the ip address in long format only
        addr_list = (struct in_addr **) he->h_addr_list;

        for(int i = 0; addr_list[i] != NULL; i++)
        {
            //strcpy(ip , inet_ntoa(*addr_list[i]) );
            server.sin_addr = *addr_list[i];

            cout<<address<<" resolved to "<<inet_ntoa(*addr_list[i])<<endl;

            break;
        }
    }

    //plain ip address
    else
    {
        server.sin_addr.s_addr = inet_addr( address.c_str() );
    }

    server.sin_family = AF_INET;
    server.sin_port = htons( port );

    //Connect to remote server
    if (connect(sock , (struct sockaddr *)&server , sizeof(server)) < 0)
    {
        perror("connect failed. Error");
        return 1;
    }

    cout<<"Connected\n";
    return true;
}

/**
    Send data to the connected host
*/
bool tcp_client::send_data(string data)
{
    //Send some data
    if( send(sock , data.c_str() , strlen( data.c_str() ) , 0) < 0)
    {
        perror("Send failed : ");
        return false;
    }
    cout<<"Data send\n";

    return true;
}

/**
    Receive data from the connected host
*/
string tcp_client::receive(int size=512)
{
    char buffer[size];
    string reply;

    //Receive a reply from the server
    if( recv(sock , buffer , sizeof(buffer) , 0) < 0)
    {
        puts("recv failed");
    }

    reply = buffer;
    return reply;
}



using namespace cv;
static map<string, int> BGRValues;
static int Person_Color[2][3] = {{28,1,2},{145,2,7}};//{ { 244,213,9},{95,95,95} };
static int rangeGap = DITHERING_DIVISION_FACTOR;
static int max1 = 0, max2 = 1, max3 = 0, max4 = 0;
static string element[4];
static float U2L=0.0;

void find_MaxValues()

{
	max1 = 0; max2 = 0; max3 = 0;max4 = 0;
	for (int i = 0; i < 4; i++)
	{
		element[i] = "";
	}
	for (auto elem : BGRValues)
	{
		int value = elem.second;
		string index = elem.first;
		if (max1 < value)
		{
			max4 = max3;
			max3 = max2;
			max2 = max1;
			max1 = value;

			element[3] = element[2];
			element[2] = element[1];
			element[1] = element[0];
			element[0] = index;

		}
		else if (max2 < value)
		{
			max4 = max3;
			max3 = max2;
			max2 = value;

			element[3] = element[2];
			element[2] = element[1];
			element[1] = index;

		}
		else if (max3< value)
		{
			max4 = max3;
			max3 = value;

			element[3] = element[2];
			element[2] = index;

		}
		else if (max4< value)
		{
			max4 = value;
			element[3] = index;

		}


	}

}

void addto_map(int B, int G, int R)
{

	string key = to_string(B) + ":" + to_string(G) + ":" + to_string(R);
	if (!BGRValues.empty())
	{

		if (BGRValues.find(key)== BGRValues.end())
		{
			BGRValues.insert(pair<string, int>(key, 1));
		}else
		{
			int value = BGRValues.find(key)->second;
			value++;
			BGRValues.find(key)->second = value;
		}
	}
	else
	{
		BGRValues.insert(pair<string,int>(key,1));

	}

}

int findMax(int color, int range)
{
    rangeGap = range;
	if (color+ rangeGap <= 255)
	{
		color += rangeGap;

	}
	else {
		color = 255;
	}
	//cout << " max color " << color << endl;
	return color;

}

int findMin(int color,int range)
{
    rangeGap = range;
	if (color- rangeGap >=0)
	{
		color -= rangeGap;

	}
	else {
		color = 0;
	}

	//cout << " min color " << color<< endl;
	return color;
}

void colorReduce(cv::Mat& image, int div = DITHERING_DIVISION_FACTOR)
{
	int nl = image.rows;                    // number of lines
	int nc = image.cols * image.channels(); // number of elements per line

	for (int j = 0; j < nl; j++)
	{
		// get the address of row j
		uchar* data = image.ptr<uchar>(j);

		for (int i = 0; i < nc; i++)
		{
			// process each pixel
			data[i] = data[i] / div * div + div / 2;
		}
	}
}


int startVideoProcess(){

    try
        {


        int keyboard = 0;
        //VideoCapture cap("/media/pi/MULTIBOOT/people.mp4");
        VideoCapture cap(0);
        Mat image,a,c;
        HOGDescriptor hog;
        hog.setSVMDetector(HOGDescriptor::getDefaultPeopleDetector());
        vector<Rect> found, found_filtered;
        Mat b;

        while ((char)keyboard != 'q' && (char)keyboard != 27) {

            a.release();
            b.release();

            c.release();


            if (!cap.isOpened())
            {
                exit(EXIT_FAILURE);
            }

            for(int i=0; i < 6;i++)
            cap >> c;

            if (c.empty())
            {
                exit(EXIT_FAILURE);

            }
        	resize(c, a, Size(), 0.25, 0.25, INTER_CUBIC);
           // a = c.clone();
            b = a.clone();
            /////////use hog descriptor /////////////
            const clock_t beginTime_1 = clock();

            hog.detectMultiScale(a, found, 0, Size(4, 4), Size(16, 16), 1.05, 2);

            //cout << "time for HOG " << float(clock() - beginTime_1) / CLOCKS_PER_SEC << endl;

            size_t i, j;
        #pragma omp parallel for
            for (i = 0; i < found.size(); i++)
            {
                Rect r = found[i];
                for (j = 0; j < found.size(); j++)
                    if (j != i && (r & found[j]) == r)
                        break;
                if (j == found.size())
                    found_filtered.push_back(r);
            }
            //vector<Mat> people;

         //   cout << "found filtered size " <<found_filtered.size() <<endl;


            for (i = 0; i < found_filtered.size(); i++)
            {
                Rect r = found_filtered[i];
                cout << r.tl().x << "+++" << r.tl().y <<endl;

                if (r.y > 0 && r.x > 0)
                {

                    rectangle(b, r.tl(), r.br(), cv::Scalar(0, 255, 0), 2);
                }

             //   cout << "Before HOG" << i<<endl;


            //   imshow("Hog ",b);
             //   cv::imwrite("HOG",b);
             //   cout << "after HOG" << i<<endl;



                cv::Mat result; // segmentation result (4 possible values)
                cv::Mat bgModel, fgModel;

                cv::grabCut(a,result,r,bgModel, fgModel,1,cv::GC_INIT_WITH_RECT);

                cv::compare(result, cv::GC_PR_FGD, result, cv::CMP_EQ);
                // Generate output image
                cv::Mat foreground(a.size(), CV_8UC3, cv::Scalar(255, 255, 255));
                a.copyTo(foreground, result);
             //   cv::imshow("Segmented Image", foreground);


                ////////secondly added part////////////////////////////////////////////////////////////////////////////////////

    //			cout << "time for grabcut " << float(clock() - beginTime) / CLOCKS_PER_SEC << endl;

                // extract the features
                Mat crop;
                Mat Fuse;


                if (0<=r.x && 0<= r.width && r.x +r.width <=foreground.cols && 0<=r.y && 0<=r.height && r.y +r.height <=foreground.rows )
                {
                    crop = foreground(r);
                    Fuse = a(r);


                }
                else
                {
                    crop = foreground;
                    Fuse = a;
                }

             //   cout << "Before crop "<<endl;
            //	imshow("Cropped image", crop);


              //  imshow("grab_cut",crop);
              //  imshow("F use",Fuse);




                //BGR values

                medianBlur(crop, crop, 5);

                //clear the static map
                if (!BGRValues.empty())
                {
                    BGRValues.clear();

                }


                colorReduce(crop);
                int RGBReduced[2][3];

                for(int k=0; k<2 ;k++)
                {
                    for(int l=0; l<3 ;l++)
                    {
                        RGBReduced[k][l] = Person_Color[k][l] / DITHERING_DIVISION_FACTOR * DITHERING_DIVISION_FACTOR+ DITHERING_DIVISION_FACTOR / 2;
                    }

                }



               // imshow("Color reduce",crop);

                //clear background

                //create a 2d array
                int **boundry = new int*[crop.rows];
                for (int i = 0; i < crop.rows; i++)
                {
                    boundry[i] = new int[2];

                }


                // get color values
                for (int i = 0; i < crop.rows; i++)
                {
                    int left = 0, right = 0, lvalidate = 0, rvalidate = 0;vector<int> v;
                    for (int j = 0, k = crop.cols - 1; j <= crop.cols / 2; j++, k--)
                    {

                        int B = crop.at<cv::Vec3b>(i, j)[0];
                        int G = crop.at<cv::Vec3b>(i, j)[1];
                        int R = crop.at<cv::Vec3b>(i, j)[2];
                        int checkColor = 224;

                        if ((B!= checkColor && G!= checkColor && R!= checkColor) && lvalidate==0)
                        {
                            left = j;
                            lvalidate = 1;
                            addto_map(B, G, R);
                            boundry[i][0] = j;


                        }else if(lvalidate==1)
                        {
                            addto_map(B, G, R);

                        }

                        int Br = crop.at<cv::Vec3b>(i, k)[0];
                        int Gr= crop.at<cv::Vec3b>(i, k)[1];
                        int Rr = crop.at<cv::Vec3b>(i, k)[2];

                        if ((Br != checkColor && Gr != checkColor && Rr != checkColor) && rvalidate == 0)
                        {
                            right = k;
                            rvalidate = 1;
                            addto_map(Br, Gr, Rr);
                            boundry[i][1] = k;
                        }
                        else if (rvalidate == 1)
                        {
                            addto_map(Br, Gr, Rr);

                        }

                    }
                    if (lvalidate == 0 && rvalidate == 1)
                    {

                        boundry[i][0] = right;

                    }
                    if (rvalidate == 0 && lvalidate == 1)
                    {
                        boundry[i][1] = left;

                    }

                }




                //find maximum colors
                find_MaxValues();


                cout << " max 1 " << element[0] << " " << max1 << " max 2 " << element[1] << " " << max2 << " max 3 " << element[2] << " " << max3 << " max 4 " << element[3] << endl;



                //detect contours



                int maxColors[4][3];


                //convert colors to rgb values
                int control = 0;
                for (int i = 0; i < 4; i++)
                {
                    istringstream iss(element[i]);
                    string s;
                    int j = 0;
                    while (getline(iss, s, ':')) {
                     /*   if (s.compare("_") ==0)
                        {
                            break;
                            control = i;
                        } */

                        maxColors[i][j]= atoi(s.c_str());
                        j++;

                    }
                }



                bool ok1 = false;
                bool ok2 = false;
            //    cout << "before 2 loop"<<endl;

                for (int i = 0; i < 4; i++)
                {
                    bool b1 = maxColors[i][0] >= findMin(RGBReduced[0][0],DITHERING_DIVISION_FACTOR) && maxColors[i][0] <= findMax(RGBReduced[0][0],DITHERING_DIVISION_FACTOR);
                    bool g1 = maxColors[i][1] >= findMin(RGBReduced[0][1],DITHERING_DIVISION_FACTOR) && maxColors[i][1] <= findMax(RGBReduced[0][1],DITHERING_DIVISION_FACTOR);
                    bool r1 = maxColors[i][2] >= findMin(RGBReduced[0][2],DITHERING_DIVISION_FACTOR) && maxColors[i][2] <= findMax(RGBReduced[0][2],DITHERING_DIVISION_FACTOR);

                    bool b2 = maxColors[i][0] >= findMin(RGBReduced[1][0],DITHERING_DIVISION_FACTOR)&& maxColors[i][0] <= findMax(RGBReduced[1][0],DITHERING_DIVISION_FACTOR);
                    bool g2 = maxColors[i][1] >= findMin(RGBReduced[1][1],DITHERING_DIVISION_FACTOR)&& maxColors[i][1] <= findMax(RGBReduced[1][1],DITHERING_DIVISION_FACTOR);
                    bool r2 = maxColors[i][2] >= findMin(RGBReduced[1][2],DITHERING_DIVISION_FACTOR)&& maxColors[i][2] <= findMax(RGBReduced[1][2],DITHERING_DIVISION_FACTOR);

                    if ((b1 && g1 && r1)||(b2 && g2 && r2))
                    {

                        if (ok1)
                        {

                            ok2 = true;
                        }
                        else
                        {
                            ok1 = true;
                            if (Person_Color[0][0] == Person_Color[1][0] && Person_Color[0][1] == Person_Color[1][1] && Person_Color[0][2] == Person_Color[1][2])
                            {

                                ok2 = true;

                            }
                        }
                    }

                }

                cout << "after 1"<< RGBReduced[0][0] << " "<< RGBReduced[0][1]<<" "<<RGBReduced[0][2]<<endl;
                cout << "after 2"<< RGBReduced[1][0] << " "<< RGBReduced[1][1]<<" "<<RGBReduced[1][2]<<endl;

                vector<Rect> bodyRect(2);
                float upper2lower = 0.0;

                // adding the extracting of HSV colors
             //   cout << "image content " << ok1 << " " << ok2 << endl;
                int colorRange[2][3][2];

                if (ok1 && ok2)
                {

                    Mat bin1;
                   // Rect rectangles[3];

                    //process single image
                    bool upperOK =false;
                    bool lowerOK =false;
                    int Colors[2][3];

                    for (int i = 0; i < 2; i++)
                    {

                        //find the range for one color
                        for (int j = 0; j <3; j++)
                        {
                            int color = RGBReduced[i][j];
                            colorRange[i][j][0] = findMin(color,DITHERING_DIVISION_FACTOR);
                            colorRange[i][j][1] = findMax(color,DITHERING_DIVISION_FACTOR);


                        }


                        //extract the color binary

                        //find the bounding rectangle
                        inRange(crop, Scalar(colorRange[i][0][0], colorRange[i][1][0], colorRange[i][2][0]), Scalar(colorRange[i][0][1], colorRange[i][1][1], colorRange[i][2][1]), bin1);

                      //  imshow("Binary Image",bin1);

                        cv::Mat colorForeground = cv::Mat::zeros(Fuse.size(), Fuse.type());
                        Fuse.copyTo(colorForeground, bin1);

                      //  imshow("Color rgb", colorForeground);
                        Mat Color_HSV;

                        cvtColor(colorForeground, Color_HSV, CV_BGR2HSV_FULL);

                       // cout << "hsv color OK"<<endl;



                        //imwrite("one_color.jpg",bin1[i]);

                        vector<vector<Point> > contours;
                        vector<Vec4i> hierarchy;

                        //
                        findContours(bin1, contours, hierarchy, CV_RETR_TREE, CV_CHAIN_APPROX_SIMPLE, Point(0, 0));

                        /// Approximate contours to polygons + get bounding rects and circles
                        vector<vector<Point> > contours_poly(contours.size());
                        vector<Rect> boundRect(contours.size());
                        vector<Point2f>center(contours.size());
                        vector<float>radius(contours.size());

                        Rect maximum_rect;
                        int size = 0;
                        int index;
                        Mat drawing = Mat::zeros(bin1.size(), CV_8UC3);
                        for (int k = 0; k < contours.size(); k++)
                        {
                            approxPolyDP(Mat(contours[k]), contours_poly[k], 3, true);
                            boundRect[k] = boundingRect(Mat(contours_poly[k]));
                            if (size< boundRect[k].area())
                            {
                                size = boundRect[k].area();
                                index = k;
                            }

                            //	cout << boundRect[k].area() << endl;

                        }

                        bodyRect[i]= boundRect[index];

                        if (!BGRValues.empty())
                        {
                            BGRValues.clear();

                        }

                        int start = boundRect[index].y;
                        int end = boundRect[index].y + boundRect[index].height;

                       // cout<< start << " "<<end <<endl;

                        for (int i = start ; i < end ; i++)
                        {

                           // for (int j = boundry[i][0]; j < boundry[i][1]; j++)
                           for(int j=boundRect[index].x ; j<boundRect[index].x + boundRect[index].width ; j++)
                            {
                          //  cout<< "Inside 2nd loop" <<endl;
                                int H = Color_HSV.at<cv::Vec3b>(i, j)[0];
                                int S = Color_HSV.at<cv::Vec3b>(i, j)[1];
                                int V = Color_HSV.at<cv::Vec3b>(i, j)[2];
                                addto_map(H, S, V);
                              //  cout << H <<" "<<S <<" "<< V <<endl;
                            }

                        }

                        find_MaxValues();
                        cout << "HSV  max 1 " << element[0] << " " << max1 << " max 2 " << element[1] << " " << max2 << " max 3 " << element[2] << " " << max3 << " max 4 " << element[3] << " " << max4 << endl;


                        //end of finding the bounding rectangle

                        //extract the forground with real colors using HSV

                        int personHSV[2][3];
                        int HsvColorRange[2][3][2];
                        for (int i = 0; i < 2; i++)
                        {
                            Mat hsv;
                            Mat rgb(1, 1, CV_8UC3, Scalar(Person_Color[i][0], Person_Color[i][1], Person_Color[i][2]));

                            cvtColor(rgb, hsv, CV_BGR2HSV_FULL);

                            HsvColorRange[i][0][0] = findMin((int)hsv.at<cv::Vec3b>(0, 0)[0],30);
                            HsvColorRange[i][0][1] = findMax((int)hsv.at<cv::Vec3b>(0, 0)[0],30);
                            HsvColorRange[i][1][0] = findMin((int)hsv.at<cv::Vec3b>(0, 0)[1],100);
                            HsvColorRange[i][1][1] = findMax((int)hsv.at<cv::Vec3b>(0, 0)[1],100);
                            HsvColorRange[i][2][0] = 0;
                            HsvColorRange[i][2][1] = 255;
                            cout << "Colors " << (int)hsv.at<cv::Vec3b>(0, 0)[0] << ":" << (int)hsv.at<cv::Vec3b>(0, 0)[1] << ":" << (int)hsv.at<cv::Vec3b>(0, 0)[2] << endl;

                        }

                        for (int i = 0; i < 4; i++)
                        {
                            istringstream iss(element[i]);
                            string s;
                            int j = 0;
                            while (getline(iss, s, ':')) {

                                if (s.compare("_") == 0)
                                {
                                    break;
                                    //control = i;
                                }

                                maxColors[i][j] = atoi(s.c_str());
                                j++;

                            }
                        }


                        for (int l = 0; l < 4; l++)
                        {
                            bool H = maxColors[l][0] >= HsvColorRange[i][0][0] && maxColors[l][0] <= HsvColorRange[i][0][1];
                            bool S = maxColors[l][1] >= HsvColorRange[i][1][0] && maxColors[l][1] <= HsvColorRange[i][1][1];
                            bool V = maxColors[l][2] >= HsvColorRange[i][2][0] && maxColors[l][2] <= HsvColorRange[i][2][1];

                        //    cout << "Bool "<<H <<" "<<S <<" "<< V<< "I : "<< i<< endl;
                        // cout << "Bool "<<S <<" "<< maxColors[l][1] <<" >= " <<HsvColorRange[i][1][0] << " && "<<maxColors[l][1] <<" <= "<<HsvColorRange[i][1][1]<<endl;
                            if (H && S && V)
                            {
                                if(i == 0)
                                {
                                    upperOK = true;

                                }else if(i== 1)
                                {
                                    lowerOK =true;
                                }
                                Colors[i][0] = maxColors[l][0];
                                Colors[i][1] = maxColors[l][1];
                                Colors[i][2] = maxColors[l][2];

                            }


                        }

                        //waitKey(0);




                    }


                    if(upperOK && lowerOK)
                        {
                            cout << " contain Both colors  " << endl;

                            // find the upper and lower are correct
                            cout << "Upper color " << Colors[0][0] << ", " << Colors[0][1] << "," << Colors[0][2] << endl;
                            cout << "Lower color " << Colors[1][0] << ", " << Colors[1][1] << "," << Colors[1][2] << endl;

                         //   imshow("Color rgb", crop);

                            cout << "Body rect " << bodyRect[0].y << " " << bodyRect[1].y << endl;

                            if (bodyRect[0].y <= bodyRect[1].y)
                            {

                                cout << "color orders matched" << endl;

                                upper2lower = (float)bodyRect[0].height / bodyRect[1].height;

                                Rect x = r;

                                if(x.x < 0){
                                    x.x = 0;
                                }
                                if(x.x+x.width > b.cols){
                                    x.width = b.cols - x.x;
                                }
                                if(x.y < 0){
                                    x.y = 0;
                                }
                                if(x.y+x.height > b.rows){
                                    x.height = b.rows - x.y;
                                }

                                vector<uchar> buf;
                                imencode(".jpg", b(x), buf);
                                uchar *enc_msg = new uchar[buf.size()];
                                for(int i=0; i < buf.size(); i++)
                                {
                                    enc_msg[i] = buf[i];
                                }
                                string encoded = base64_encode(enc_msg, buf.size());

                                tcp_client client;
                                string host="localhost";



                            //connect to host
                                client.conn(host , 8080);



                                stringstream ss;
                                ss << encoded.size();
                                client.send_data(ss.str());
                                client.receive(1024);
                                client.send_data(encoded);






                            }else
                            {
                                cout << "color orders doesnt match"<< endl;
                            }

                            cout << "Upper to lower " << upper2lower << endl;







                        //waitKey(0);

                    //receive and echo reply
                        cout<<"----------------------------\n\n";
                    //cout<<c.receive(1024);
                        cout<<"\n\n----------------------------\n\n";

                    //done



                            //end of finding upper and lower correct
                        }
                        else if(upperOK )
                        {
                            cout << "Upper Color Matched" << endl;
                        }
                        else if (lowerOK)
                        {
                            cout << "Lower Color Matched " << endl;
                        }
                        else
                        {
                            cout << "Both colors didn't match" << endl;
                        }


                }


        //client code ends here


                keyboard = waitKey(1);



                }

                found_filtered.clear();


            }
        }
        catch(...)
        {

            return 1;
        }

    return 0;

}



int main()
{


    boost::thread t;
    //server code starts here


    int listenfd=0,connfd=0;

    //create a variable in unix sockaddr structure
    struct sockaddr_in serv_addr;
    char sendBuff[1025];
    int numrv;

    //access file descrptor of sockets using ipv4
    listenfd=socket(AF_INET,SOCK_STREAM,0);
    printf("socket retrieve success\n");

    memset(&serv_addr,'0', sizeof(serv_addr));
    memset(sendBuff,'0',sizeof(sendBuff));

    serv_addr.sin_family=AF_INET;
    //use any inetrnet interface and use byte ordering
    serv_addr.sin_addr.s_addr=htonl(INADDR_ANY);
    serv_addr.sin_port=htons(50001);

    //bind the socket
    bind(listenfd, (struct sockaddr*)&serv_addr,sizeof(serv_addr));

    if(listen(listenfd,10)==-1)
    {
        printf("Failed to listen\n");
        return -1;
    }

    connfd = accept(listenfd,(struct sockaddr*)NULL,NULL);
    //wait always for new connctions
    while(true){


    //Receive a message from client
//            while( (read_size = recv(client_sock , client_message , 2000 , 0)) > 0 )
//            {
//                //Send the message back to client
//                write(client_sock , client_message , strlen(client_message));
//            }
        char msg_client[100];

        recv(connfd , msg_client , 100 , 0);
        cout << "client msg "<<msg_client<<endl;


        strcpy(sendBuff,"Message Form Server >> ");
        strcat(sendBuff,msg_client);
        write(connfd, sendBuff, strlen(sendBuff));

        // split the message to data
      //  string msg = "223:14:67:56:45:35:1.02";
        istringstream iss(msg_client);
        string s;
        int j = 0,i=0;
        while (getline(iss, s, ':')) {
		if (i<2)
            {
                Person_Color[i][j] = atoi(s.c_str());
            }
            if (i==2)
            {
                U2L = atof(s.c_str());

            }

            j++;
            if (j == 3)
            {
                i++;
                j = 0;

            }

	}

	cout << Person_Color[0][0] << " " << Person_Color[0][1] << " " << Person_Color[0][2] << endl;
	cout << Person_Color[1][0] << " " << Person_Color[1][1] << " " << Person_Color[1][2] << endl;
	cout << "U2L " << U2L << endl;


        // end of spliting



        if(!strcmp(msg_client,"stop")){
        cout << "stop"<<endl;
             t.interrupt();
             break;
        }else{
        cout << "start"<<endl;
            t = boost::thread(&startVideoProcess);
        }
        bzero(msg_client, sizeof(msg_client));

    }



    cout << "Thread is stopped" << endl;




    //server code ends here




 return 0;
}
