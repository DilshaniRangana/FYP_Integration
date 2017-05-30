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
static int Person_Color[2][3] = { {224,224,160},{ 32,32,32 } };
static int rangeGap = 3;


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

int findMax(int color)
{
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

int findMin(int color)
{
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

void colorReduce(cv::Mat& image, int div = 64)
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



int main()
{

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

    //wait always for new connctions

    connfd = accept(listenfd,(struct sockaddr*)NULL,NULL);
    //Receive a message from client
//            while( (read_size = recv(client_sock , client_message , 2000 , 0)) > 0 )
//            {
//                //Send the message back to client
//                write(client_sock , client_message , strlen(client_message));
//            }
    char msg_client[100];
    recv(connfd , msg_client , 100 , 0);
    cout << "client msg "<<msg_client<<endl;


    strcpy(sendBuff,"Message Form Server\n");
    write(connfd, sendBuff, strlen(sendBuff));


    close(connfd);


            //server code ends here





    int keyboard = 0;
	VideoCapture cap("/media/pi/MULTIBOOT/people.mp4");
	//VideoCapture cap(0);
	Mat image,a,b,d,c;
    HOGDescriptor hog;
    hog.setSVMDetector(HOGDescriptor::getDefaultPeopleDetector());
    vector<Rect> found, found_filtered;

    while ((char)keyboard != 'q' && (char)keyboard != 27)
        {
            if (!cap.isOpened())
            {
                cout <<"not opend"<<endl;

                exit(EXIT_FAILURE);
            }

            //for(int i=0; i < 6;i++)
            cap >> c ;

            if (c.empty())
            {
                cout<< "frame empty"<<endl;
                //exit(EXIT_FAILURE);
                continue;

            }


            resize(c, a, Size(), 0.25, 0.25, INTER_CUBIC);

            const clock_t beginTime_1 = clock();

            hog.detectMultiScale(a, found, 0, Size(4, 4), Size(12, 12), 1.05, 2);

            cout << "time for HOG " << float(clock() - beginTime_1) / CLOCKS_PER_SEC << endl;
          //  imwrite("Original.jpg",a);

            for (int i = 0; i < found.size(); i++){

                Mat x = a.clone();
                rectangle(a, found[i], cv::Scalar(0, 255, 0), 2);
                cv::Mat result; // segmentation result (4 possible values)
                cv::Mat bgModel, fgModel;

                Rect r = found[i];

                cv::grabCut(x,result,r,bgModel, fgModel,1,cv::GC_INIT_WITH_RECT);

                cv::compare(result, cv::GC_PR_FGD, result, cv::CMP_EQ);
                // Generate output image
                cv::Mat foreground(x.size(), CV_8UC3, cv::Scalar(255, 255, 255));
                a.copyTo(foreground, result);

                Mat crop = foreground(r);

               // imwrite("Grab_CUt.jpg",crop);

                medianBlur(crop, crop, 5);

                if (!BGRValues.empty())
                {
                    BGRValues.clear();

                }

                colorReduce(crop);

             //   cv::imshow("Color Reduction", crop);

              //  imwrite("reduced.jpg",crop);

                int **boundry = new int*[crop.rows];
                for (int i = 0; i < crop.rows; i++)
                {
                    boundry[i] = new int[2];

                }
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

                int max1 = 0, max2 = 1, max3 = 0,max4=0;
                string element[4];

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
                        element[0]  = index;

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

                cout << " max 1 " << element[0] << " " << max1 << " max 2 " << element[1] << " " << max2 << " max 3 " << element[2] << " " << max3 << " max 4 " << element[3] << endl;

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

                int colorRange[4][3][2];

                bool ok1 = false;
                bool ok2 = false;

                for (int i = 0; i < 4; i++)
                {

                    if ((maxColors[i][0] == Person_Color[0][0] && maxColors[i][1] == Person_Color[0][1] && maxColors[i][2] == Person_Color[0][2]) || (maxColors[i][0] == Person_Color[1][0] && maxColors[i][1] == Person_Color[1][1] && maxColors[i][2] == Person_Color[1][2]))
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

                cout << "image content " << ok1 << " " << ok2 << endl;

                			if (ok1 && ok2)
			{

				Mat bin1[3];
				Rect rectangles[3];

				//process single image

				for (int i = 0; i < 2; i++)
				{

					//find the range for one color
					for (int j = 0; j <3; j++)
					{
						int color = Person_Color[i][j];
						colorRange[i][j][0] = findMin(color);
						colorRange[i][j][1] = findMax(color);

					}


					//extract the color binary
					inRange(crop, Scalar(colorRange[i][0][0], colorRange[i][1][0], colorRange[i][2][0]), Scalar(colorRange[i][0][1], colorRange[i][1][1], colorRange[i][2][1]), bin1[i]);


					imshow("one color ", bin1[i]);
					//imwrite("one_color.jpg",bin1[i]);

					vector<vector<Point> > contours;
					vector<Vec4i> hierarchy;

					//
					findContours(bin1[i], contours, hierarchy, CV_RETR_TREE, CV_CHAIN_APPROX_SIMPLE, Point(0, 0));

					/// Approximate contours to polygons + get bounding rects and circles
					vector<vector<Point> > contours_poly(contours.size());
					vector<Rect> boundRect(contours.size());
					vector<Point2f>center(contours.size());
					vector<float>radius(contours.size());

					Rect maximum_rect;
					int size = 0;
					int index;
					Mat drawing = Mat::zeros(bin1[i].size(), CV_8UC3);
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
					//draw the rectangle
					rectangles[i] = boundRect[index];
					rectangle(drawing, boundRect[index].tl(), boundRect[index].br(), Scalar(0, 255, 0), 2, 8, 0);
					imshow("rect ", drawing);
					//imwrite("rect.jpg", drawing);

					waitKey(0);






					cout << Person_Color[i][2] << "," << Person_Color[i][1] << "," << Person_Color[i][0] << endl;




				}

				int xCordinates[3];
				int yCordinates[3];
				int maxX = 0, xind;
				int maxY = 0, yind;

				for (int i = 0; i < sizeof(rectangles)/sizeof(Rect); i++)
				{
					xCordinates[i] = rectangles[i].tl().x;
					if (maxX < rectangles[i].tl().x)
					{
						maxX = rectangles[i].tl().x;
						xind = i;
					}

					if (maxY < rectangles[i].tl().y)
					{
						maxY = rectangles[i].tl().y;
						yind = i;
					}


					yCordinates[i] = rectangles[i].tl().y;
				}

				//if upper color = lower color code goes here





				//if upper color = lower color code ends here



				//upperbody color
				int Lb = Person_Color[yind][0];
				int Lg = Person_Color[yind][1];
				int Lr = Person_Color[yind][2];

				cout << "lower body color " << Lb << "," << Lg << "," << Lr << endl;
			//	Mat img(500, 500, CV_8UC3);
			//	img = cv::Scalar(Lb, Lg, Lr);
			//	imshow("Lower body ", img);
				//imwrite("Lower_body.jpg ", img);

				int upper = 2;
				if (yind == 0)
				{
					upper = 1;
				}
				else
				{
					upper = 0;
				}

				 Lb = Person_Color[upper][0];
				 Lg = Person_Color[upper][1];
				 Lr = Person_Color[upper][2];


			//	Mat img1(500, 500, CV_8UC3);
			//	img1 = cv::Scalar(Lb, Lg, Lr);
			//	imshow("Upper body ", img1);
				//imwrite("Upper_body.jpg ", img1);



			}













                 /////////// code to encode image with base64////////////////////////

                vector<uchar> buf;
                imencode(".jpg", a, buf);
                uchar *enc_msg = new uchar[buf.size()];
                for(int i=0; i < buf.size(); i++)
                {
                    enc_msg[i] = buf[i];
                }
                string encoded = base64_encode(enc_msg, buf.size());
                cout<<encoded<<endl;
             //   imshow("capture ",d);
              //  waitKey(1);

                /////////// end of code to encode image with base64////////////////////////



                    //client code starts here

                tcp_client c;
                string host="localhost";



                //connect to host
               c.conn(host , 8080);

                //send some data
              // c.send_data("Dilshani is the genious");

              // imwrite("test.jpg",d);

               stringstream ss;
               ss << encoded.size();
               c.send_data(ss.str());
               c.receive(1024);
               c.send_data(encoded);

               waitKey(0);

                //receive and echo reply
                cout<<"----------------------------\n\n";
                //cout<<c.receive(1024);
                cout<<"\n\n----------------------------\n\n";

                //done



    //client code ends here

            }
            a.release();
            b.release();
            c.release();
            keyboard = waitKey(1);
        }

 return 0;
}
