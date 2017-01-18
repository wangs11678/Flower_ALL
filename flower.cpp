/*
* Flower Classification
* 方法：Bag of Words
* 步骤描述
* 1. 提取训练集中图片的feature。
* 2. 将这些feature聚成n类。这n类中的每一类就相当于是图片的“单词”，
*    所有的n个类别构成“词汇表”。本文中n取1000，如果训练集很大，应增大取值。
* 3. 对训练集中的图片构造Bag of Words，就是将图片中的feature归到不同的类中，
*    然后统计每一类的feature的频率。这相当于统计一个文本中每一个单词出现的频率。
* 4. 训练一个多类分类器，将每张图片的Bag of Words作为feature vector，
*    将该张图片的类别作为label。
* 5. 对于未知类别的图片，计算它的Bag of Words，使用训练的分类器进行分类。
*/

#include "flower.h"  

float result = 0; //the result of the svm, 1:red 2:white 3:bud 

int cmdNo1 = 101; //命令编号（发送数据）
int cmdNo2 = 102; //命令编号（发送图片）
int devNo = 0; //设备号
int messageID = 0; //消息ID
float message = 0; //传输result数据
char picturePath[40]; //传输图片path
char pend[10] = "pend"; //包结束符

struct parameter
{
	char ip[20];
	int port;
};

void sendResultOrPicture(int sockClient, int cmdNo, int devNo, int msgID, float message, char* picPath, char* end)
{
/********************************send result*****************************************/	
    char buff[100] = {0};

    if(cmdNo==101)
    {	
        DataPackage *dp = (DataPackage*)malloc(sizeof(DataPackage));        
		dp->packageLen = 24;
		dp->commandNo = cmdNo;
		dp->deviceNo = devNo;
		dp->messageID = msgID;
		dp->message = message;
		dp->pend = end;

		cout<<"Flower Classification: Send data..."<<endl;
       	//发送数据  
        int ret = send(sockClient, (char*)dp, sizeof(struct DataPackage), 0); 
 		if (ret == 0)
 		{
        	printf("Flower Classification: Send data failed!\n");
			return;
       	}
		//接收服务器回应  
		cout<<"Flower Classification: Server Response: ";      
        recv(sockClient, buff, sizeof(buff), 0);  
        cout<<buff<<endl;  
		cout<<"Flower Classification: Send data finished."<<endl;
		free(dp);		
    }
/********************************send picture*****************************************/
	else if(cmdNo==102)
	{
        FILE *fp;
		fp=fopen(picPath, "rb+");
		fseek(fp, 0, SEEK_END);
		int fend=ftell(fp);
	
		fseek(fp, 0, 0);		
        PicPackage *pp = (PicPackage*)malloc(sizeof(PicPackage));
		pp->packageLen = fend+20;
		pp->commandNo = cmdNo; //命令编号
		pp->deviceNo = devNo; //设备编号
		pp->messageID = msgID; //消息ID
		//pp.picture = &msg[0]; //图片数据
		pp->pend = end; //包结束符			
		bzero(pp->picture, sizeof(pp->picture));              
        fread(pp->picture, fend, 1, fp);	
        cout<<"Flower Classification: Send picture..."<<endl;      				               
        int ret=send(sockClient, (char*)pp, pp->packageLen, 0); //将包发送给服务器               
        if (ret == 0){
        	printf("Flower Classification: Send picture failed!\n");
			return;
       	}
		//接收服务器回应  
		cout<<"Flower Classification: Server Response: ";
        recv(sockClient, buff, sizeof(buff), 0);  
        cout<<buff<<endl;  
		cout<<"Flower Classification: Send picture finished."<<endl; 
		fclose(fp);
		free(pp);              			
	}
}


void * thread_result_client(void *addr)
{
	struct parameter *p;
	p = (struct parameter*)addr;
	int client_fd;
	if(-1 == (client_fd=socket(AF_INET, SOCK_STREAM, 0))){
		perror("Flower Classification: Create Socket Failed!\n");
		return 0;
	}
	
	struct sockaddr_in client_socket;
	memset(&client_socket, 0, sizeof(client_socket));
	client_socket.sin_family = AF_INET;
	client_socket.sin_port = htons(p->port);
	
	if(0 == inet_aton(p->ip, &client_socket.sin_addr)){
		perror("Flower Classification: Server IP Address Error!\n");
		return 0;
	}
	
	if(-1 == connect(client_fd, (struct sockaddr*) &client_socket, sizeof(client_socket))){
		perror("Flower Classification: Can Not Connect to Server!\n");
		return 0;
	}

    //发送结果数据or图片 
    sendResultOrPicture(client_fd, cmdNo2, devNo, messageID, message, picturePath, pend);
	sendResultOrPicture(client_fd, cmdNo1, devNo, messageID, message, picturePath, pend);
    
	close(client_fd);
}


Mat BuildVocabulary(const string& databaseDir, 
					const vector<string>& categories, 
					const Ptr<FeatureDetector>& detector, 
					const Ptr<DescriptorExtractor>& extractor,
					int wordCount)
{
	Mat allDescriptors;
	for (int index = 0; index != categories.size(); ++index)
	{
		cout << "processing category " << categories[index] << endl;
		string currentCategory = databaseDir + '/' + categories[index];
       	vector<string> filelist;
        getfilelist((char *)currentCategory.c_str(), filelist);
		
		cout<<(char *)currentCategory.c_str()<<endl;

    
	   	for (int j = 0; j != filelist.size(); j += 10)
		{			
			string filepath = currentCategory + '/' + filelist[j];
			Mat image = imread(filepath);
			if (image.empty())
			{
				continue; // maybe not an image file
			}
			vector<KeyPoint> keyPoints;
			Mat descriptors;
			detector -> detect(image, keyPoints);
			extractor -> compute(image, keyPoints, descriptors);
			if (allDescriptors.empty())
			{
				allDescriptors.create(0, descriptors.cols, descriptors.type());
			}
			allDescriptors.push_back(descriptors);
		}
        
		cout << "done processing category " << categories[index] << endl;
	}
	assert(!allDescriptors.empty());
	cout << "build vocabulary..." << endl;
	BOWKMeansTrainer bowTrainer(wordCount);
	Mat vocabulary = bowTrainer.cluster(allDescriptors);
	cout << "done build vocabulary..." << endl;
	return vocabulary;
}

// bag of words of an image as its descriptor, not keypoint descriptors
void ComputeBowImageDescriptors(const string& databaseDir,
								const vector<string>& categories, 
								const Ptr<FeatureDetector>& detector,
								Ptr<BOWImgDescriptorExtractor>& bowExtractor,
								const string& imageDescriptorsDir,
								map<string, Mat>* samples)
{	
	for (int i = 0; i != categories.size(); ++i)
	{
		string currentCategory = databaseDir + '/' + categories[i];
		vector<string> filelist;

		getfilelist((char *)currentCategory.c_str(), filelist);
		
	    for (int j =0; j != filelist.size(); ++j)
		{
			string descriptorFileName = imageDescriptorsDir + '/' + categories[i] + '/' + filelist[j] + ".xml.gz";
			//MakeDir(imageDescriptorsDir + "/" + categories[i]);
			FileStorage fs(descriptorFileName, FileStorage::READ);
			Mat imageDescriptor;
			if (fs.isOpened())
			{ 
				// already cached
				fs["imageDescriptor"] >> imageDescriptor;
			} 
			else
			{
				cout<<"Computing the bag of words of class "<<categories[i]<<endl;

				string filepath = currentCategory + '/' + filelist[j];
				Mat image = imread(filepath);
				if (image.empty())
				{
					continue; // maybe not an image file
				}
				vector<KeyPoint> keyPoints;
				detector -> detect(image, keyPoints);
				bowExtractor -> compute(image, keyPoints, imageDescriptor);
				fs.open(descriptorFileName, FileStorage::WRITE);
				if (fs.isOpened())
				{
					fs << "imageDescriptor" << imageDescriptor;
				}
			}
			//判断samples的string中有无categories[i].（samples是map<string, Mat>*）
			if (samples -> count(categories[i]) == 0)
			{
				(*samples)[categories[i]].create(0, imageDescriptor.cols, imageDescriptor.type());
			}
			(*samples)[categories[i]].push_back(imageDescriptor);
		}
	}
}

void TrainSvm(map<string, Mat>& samples, const string& category, const CvSVMParams& svmParams, CvSVM* svm)
{
	Mat allSamples(0, samples.at(category).cols, samples.at(category).type());
	Mat responses(0, 1, CV_32SC1);
	//assert(responses.type() == CV_32SC1);
	allSamples.push_back(samples.at(category));
	Mat posResponses(samples.at(category).rows, 1, CV_32SC1, Scalar::all(1)); 
	responses.push_back(posResponses);
	
	for (map<string, Mat>::iterator itr = samples.begin(); itr != samples.end(); ++itr)
	{
		if (itr -> first == category)
		{
			continue;
		}
		allSamples.push_back(itr -> second);
		Mat response(itr -> second.rows, 1, CV_32SC1, Scalar::all( -1 ));
		responses.push_back(response);		
	}
	svm -> train(allSamples, responses, Mat(), Mat(), svmParams);
}

// using 1-vs-all method, train an svm for each category.
// choose the category with the biggest confidence
string ClassifyBySvm(const Mat& queryDescriptor, map<string, Mat>& samples, const string& svmDir)
{
	string category;
	SVMParams svmParams;
	int sign = 0; //sign of the positive class
	float confidence = -FLT_MAX;
	for (map<string, Mat>::iterator itr = samples.begin(); itr != samples.end(); ++itr)
	{
		CvSVM svm;
		string svmFileName = svmDir + '/' + itr -> first + ".xml.gz";
		FileStorage fs(svmFileName, FileStorage::READ);
		if (fs.isOpened())
		{ 
			// exist a previously trained svm
			fs.release();
			svm.load(svmFileName.c_str());
		} 
		else
		{
			TrainSvm(samples, itr->first, svmParams, &svm);
			if (!svmDir.empty())
			{
				svm.save(svmFileName.c_str());
			}
		}
		// determine the sign of the positive class
		if (sign == 0)
		{
			float scoreValue = svm.predict(queryDescriptor, true);
			float classValue = svm.predict(queryDescriptor, false);
			sign = (scoreValue < 0.0f) == (classValue < 0.0f)? 1 : -1;
		}
		float curConfidence = sign * svm.predict(queryDescriptor, true);
		if (curConfidence > confidence)
		{
			confidence = curConfidence;
			category = itr -> first;
		}
	}
	return category;
}


void* thread_flower_classification(void *addr)
{
	Params params;
	//string method="svm";
	string databaseDir = "./data/train";
	string testPicturePath = "./data/test";
	string resultDir = "./result";
	
	cv::initModule_nonfree();

	string bowImageDescriptorsDir = resultDir + kBowImageDescriptorsDir;
	string svmsDir = resultDir + kSvmsDirs;
		
	vector<string> categories;
	
	getdirlist((char *)databaseDir.c_str(),categories);

	Ptr<FeatureDetector> detector = FeatureDetector::create(params.descriptorType);
	Ptr<DescriptorExtractor> extractor = DescriptorExtractor::create(params.descriptorType);
	Ptr<DescriptorMatcher> matcher = DescriptorMatcher::create(params.matcherType);
	if (detector.empty() || extractor.empty() || matcher.empty())
	{
		cout << "feature detector or descriptor extractor or descriptor matcher cannot be created.\nMaybe try other types?" << endl;
	}
	
	Mat vocabulary;
	string vocabularyFile = resultDir + '/' + kVocabularyFile;

	//OpenCV FileStorage类读(写)XML/YML文件
	FileStorage fs(vocabularyFile, FileStorage::READ);

	if (fs.isOpened())
	{
		//将保存在fs对象指定yml文件下的vocabulary标签下的数据读到vocabulary矩阵
		fs["vocabulary"] >> vocabulary;
	} 
	else
	{
		vocabulary = BuildVocabulary(databaseDir, categories, detector, extractor, params.wordCount);
		FileStorage fs(vocabularyFile, FileStorage::WRITE);
		if (fs.isOpened())
		{
			//将vocabulary矩阵保存在fs对象指定的yml文件的vocabulary标签下
			fs << "vocabulary" << vocabulary;
		}
	}
	Ptr<BOWImgDescriptorExtractor> bowExtractor = new BOWImgDescriptorExtractor(extractor, matcher);
	bowExtractor -> setVocabulary(vocabulary);

	//Samples这个map的key就是某个类别，value就是这个类别中所有图片的bag of words
	map<string, Mat> samples;//key: category name, value: histogram
	
	ComputeBowImageDescriptors(databaseDir, categories, detector, bowExtractor, bowImageDescriptorsDir,  &samples);
	
	vector<string> testCategories;
		
    getdirlist((char *)testPicturePath.c_str(), testCategories);
	int sum = 0;
	int right = 0;
	for (int i = 0; i != testCategories.size(); ++i)
	{		
		string currentCategory = testPicturePath + '/' + testCategories[i];
		vector<string> filelist;

        getfilelist((char *)currentCategory.c_str(), filelist);

	    for (int j=0; j != filelist.size(); ++j)
		{			
			string filepath = currentCategory + '/' + filelist[j];
                        
			Mat image = imread(filepath);
			cout << "Flower Classification: Classify image " << filelist[j] << "." << endl;
			
			vector<KeyPoint> keyPoints;
			detector -> detect(image, keyPoints);
			Mat testPictureDescriptor;
			bowExtractor -> compute(image, keyPoints, testPictureDescriptor);
			string category;
		  
			category = ClassifyBySvm(testPictureDescriptor, samples, svmsDir);

			if(category == testCategories[i])
			{
				right++;
			}
			sum++;
			cout << "Flower Classification: Pred: " << category << "." << endl;
          
			if( category=="redFlower"){
            	result=1;
            }
			else if(category=="whiteFlower"){
                result=2;
            }
			else if(category=="budFlower"){
                result=3;
            }  
			//cout<<"the path is "<<filepath<<endl;      		                  
            //cout<<"the result is "<<result<<endl; 
			//cout<<i<<endl; 
            /////////////////////set the package value////////////////////////////////////			
            devNo = 1; //设备号
			messageID = messageID+1; //消息ID
			message = result;
            strcpy(picturePath, filepath.c_str()); 
             
            //cout << "Send Result to Server..." << endl;
            ////////////create thread/////////////////        
            pthread_t id;
            int ret;               
			ret=pthread_create(&id, NULL, thread_result_client, addr);
			if(ret!=0){
				cout<<"Flower Classification: create pthread error"<<endl;
				return 0;
			}
			pthread_join(id, NULL);

			//destroyAllWindows();
			//string info = "pred: " + category;					
			//imshow(info, image);
			//cvWaitKey(1000); // 暂停0.1s显示图像			
			cout<<endl;		
		}	
	}
	cout<<"Flower Classification: Total test image: "<<sum<<endl;
	cout<<"Flower Classification: Correct prediction: "<<right<<endl;	
	cout<<"Flower Classification: Accuracy: "<<(double(right)/sum)<<endl;
	return 0;
}

void getdirlist(char *path, vector<string> &dirlist){
    
     struct dirent* ent = NULL;
     DIR *pDir;
     pDir=opendir(path);
     while (NULL != (ent=readdir(pDir)))
     {
        if (ent->d_type==4)
         {
          
            dirlist.push_back(ent->d_name);
         }
     }
     return;
}

void getfilelist(char *path, vector<string> &filelist){   
     struct dirent* ent = NULL;
     DIR *pDir;
     pDir=opendir(path);   
     while (NULL != (ent=readdir(pDir)))
     {
        if (ent->d_type==8)
         {         
            filelist.push_back(ent->d_name);
         }
     }
     return;
}
