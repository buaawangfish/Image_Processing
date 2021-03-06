#include <iostream>
#include <fstream>
#include <opencv2/opencv.hpp>
#include <stdio.h>
#include "dataset.h"
#include "my_svm.h"

using namespace std;
using namespace cv;

int HardExampleCount = 0;

int main(int argc, char** argv)
{
  Mat src;
	string ImgName;

	char saveName[256];//找出来的HardExample图片文件名
	ifstream fin("INRIANegativeImageList.txt");//打开原始负样本图片文件列表

  //检测窗口(64,128),块尺寸(16,16),块步长(8,8),cell尺寸(8,8),直方图bin个数9
	//HOGDescriptor hog(Size(64,128),Size(16,16),Size(8,8),Size(8,8),9);//HOG检测器，用来计算HOG描述子的
	int DescriptorDim;//HOG描述子的维数，由图片大小、检测窗口大小、块大小、细胞单元中直方图bin个数决定
	MySVM svm;//SVM分类器
  svm.load("SVM_HOG.xml");

  /*************************************************************************************************
	线性SVM训练完成后得到的XML文件里面，有一个数组，叫做support vector，还有一个数组，叫做alpha,有一个浮点数，叫做rho;
	将alpha矩阵同support vector相乘，注意，alpha*supportVector,将得到一个列向量。之后，再该列向量的最后添加一个元素rho。
	如此，变得到了一个分类器，利用该分类器，直接替换opencv中行人检测默认的那个分类器（cv::HOGDescriptor::setSVMDetector()），
	就可以利用你的训练样本训练出来的分类器进行行人检测了。
	***************************************************************************************************/
	DescriptorDim = svm.get_var_count();//特征向量的维数，即HOG描述子的维数
	int supportVectorNum = svm.get_support_vector_count();//支持向量的个数
	cout<<"支持向量个数："<<supportVectorNum<<endl;

	Mat alphaMat = Mat::zeros(1, supportVectorNum, CV_32FC1);//alpha向量，长度等于支持向量个数
	Mat supportVectorMat = Mat::zeros(supportVectorNum, DescriptorDim, CV_32FC1);//支持向量矩阵
	Mat resultMat = Mat::zeros(1, DescriptorDim, CV_32FC1);//alpha向量乘以支持向量矩阵的结果

	//将支持向量的数据复制到supportVectorMat矩阵中
	for(int i=0; i<supportVectorNum; i++)
	{
		const float * pSVData = svm.get_support_vector(i);//返回第i个支持向量的数据指针
		for(int j=0; j<DescriptorDim; j++)
		{
			//cout<<pData[j]<<" ";
			supportVectorMat.at<float>(i,j) = pSVData[j];
		}
	}

	//将alpha向量的数据复制到alphaMat中
	double * pAlphaData = svm.get_alpha_vector();//返回SVM的决策函数中的alpha向量
	for(int i=0; i<supportVectorNum; i++)
	{
		alphaMat.at<float>(0,i) = pAlphaData[i];
	}

	//计算-(alphaMat * supportVectorMat),结果放到resultMat中
	//gemm(alphaMat, supportVectorMat, -1, 0, 1, resultMat);//不知道为什么加负号？
	resultMat = -1 * alphaMat * supportVectorMat;

	//得到最终的setSVMDetector(const vector<float>& detector)参数中可用的检测子
	vector<float> myDetector;
	//将resultMat中的数据复制到数组myDetector中
	for(int i=0; i<DescriptorDim; i++)
	{
		myDetector.push_back(resultMat.at<float>(0,i));
	}
	//最后添加偏移量rho，得到检测子
	myDetector.push_back(svm.get_rho());
	cout<<"检测子维数："<<myDetector.size()<<endl;
	//设置HOGDescriptor的检测子
	HOGDescriptor myHOG;
	myHOG.setSVMDetector(myDetector);
	//myHOG.setSVMDetector(HOGDescriptor::getDefaultPeopleDetector());

//	//保存检测子参数到文件
//	ofstream fout("HOGDetectorForOpenCV.txt");
//	for(int i=0; i<myDetector.size(); i++)
//	{
//		fout<<myDetector[i]<<endl;
//	}

//  namedWindow("people detector", 1);

  while(getline(fin,ImgName))
  {
    cout<<"处理："<<ImgName<<endl;
    ImgName = "INRIAPerson/Train/neg/" + ImgName;
    src = imread(ImgName,1);//读取图片

      vector<Rect> found, found_filtered;
      //double t = (double)getTickCount();
      // run the detector with default parameters. to get a higher hit-rate
      // (and more false alarms, respectively), decrease the hitThreshold and
      // groupThreshold (set groupThreshold to 0 to turn off the grouping completely).
      myHOG.detectMultiScale(src, found, 0, Size(8,8), Size(32,32), 1.05, 2);
      //t = (double)getTickCount() - t;
      //printf("tdetection time = %gms\n", t*1000./cv::getTickFrequency());
      size_t i, j;
      for( i = 0; i < found.size(); i++ )
      {
          Rect r = found[i];
          for( j = 0; j < found.size(); j++ )
              if( j != i && (r & found[j]) == r)
                  break;
          if( j == found.size() )
              found_filtered.push_back(r);
      }

      for( i = 0; i < found_filtered.size(); i++ )
      {
          Rect r = found_filtered[i];
          // the HOG detector returns slightly larger rectangles than the real objects.
          // so we slightly shrink the rectangles to get a nicer output.
          //r.x += cvRound(r.width*0.1);
          //r.width = cvRound(r.width*0.8);
          //r.y += cvRound(r.height*0.07);
          //r.height = cvRound(r.height*0.8);
          if(r.x < 0)
            r.x = 0;
          if(r.y < 0)
            r.y = 0;
          if(r.x + r.width > src.cols)
            r.width = src.cols - r.x;
          if(r.y + r.height > src.rows)
            r.height = src.rows - r.y;
          Mat imgROI = src(Rect(r.x, r.y, r.width, r.height));
          resize(imgROI,imgROI,Size(64,128));
          sprintf(saveName,"dataset/HardExample/hardexample%06d.jpg",++HardExampleCount);
          imwrite(saveName,imgROI);
          //rectangle(src, r.tl(), r.br(), cv::Scalar(0,255,0), 3);
      }
      //imshow("people detector", src);
      //waitKey(0);
  }

  cout<<"HardExampleCount: "<<HardExampleCount<<endl;

  return 0;
}
