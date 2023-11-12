#include "TFTPCompression.h"
void readBinaryFile(const char* filename) {
    // Open the binary file for reading
    std::ifstream binaryFile(filename, std::ios::binary);
    if (binaryFile.is_open()) {

    

    // Open a text file for writing
    std::string strFilename(filename);
    std::string filenameWithoutExtension = strFilename.substr(0, strFilename.find_last_of("."));
    std::ofstream textFile(filenameWithoutExtension + ".txt");

    // std::ofstream textFile(std::string(filename) + ".txt");
    if (textFile.is_open()) {

    

    // Variable to store unpacked bits
    char untrackedBits;
    // Variable to keep track of the bit position in the unpacked byte
    int bitIndex = 0;

    // Read the file byte by byte
    while (binaryFile.read(&untrackedBits, sizeof(char))) {
        // Iterate through the bits in the byte
        int i =0;
        while(i<8){
            // Extract the i-th bit from the byte
            char bit = (untrackedBits & (1 << i)) ? '1' : '0';

            // Write the bit to the text file
            textFile << bit;
            // std::cout<< bit;

            // Move to the next bit position
            bitIndex++;

            // Output a space for better readability
            // if (bitIndex % 8 == 0)
            //     textFile << ' ';
            i++;

        }
        // for (int i = 0; i < 8; i++) {
        //     // Extract the i-th bit from the byte
        //     char bit = (untrackedBits & (1 << i)) ? '1' : '0';

        //     // Write the bit to the text file
        //     textFile << bit;
        //     // std::cout<< bit;

        //     // Move to the next bit position
        //     bitIndex++;

        //     // Output a space for better readability
        //     // if (bitIndex % 8 == 0)
        //     //     textFile << ' ';
        // }
    }
    }
    else{
        std::cerr << "Not able to open text file." << std::endl;
        return;

    }
    textFile.close();
    }

    else{
        std::cerr << "Not able to open binary file." << std::endl;
        return;

    }

    // Close the binary and text files
    binaryFile.close();
    
}

//plag
std::string addBinary(std::string a, std::string b)
{
    std::string result = "";
    int carry = 0;

    // Make sure both strings have the same length by adding leading shuniyas if needed
    while (a.length() < b.length())
    {
        a = "0" + a;
    }
    while (b.length() < a.length())
    {
        b = "0" + b;
    }

    for (int i = a.length() - 1; i >= 0; i--)
    {
        int bit1 = a[i] - '0'; // Convert character to integer (0 or 1)
        int bit2 = b[i] - '0'; // Convert character to integer (0 or 1)

        int sum = bit1 + bit2 + carry;
        carry = sum / 2;
        result = std::to_string(sum % 2) + result;
    }

    if (carry > 0)
    {
        result = "1" + result;
    }

    return result;
}


void combineBinaryFiles(const std::string& file1Path, const std::string& file2Path, const std::string& outputPath) {
    // Read the contents of the first binary file
    std::ifstream file1(file1Path, std::ios::binary | std::ios::ate);
    if (!file1.is_open()) {
        std::cerr << "Error opening file: " << file1Path << std::endl;
        return;
    }
    std::streamsize fileSize1 = file1.tellg();
    file1.seekg(0, std::ios::beg);
    std::vector<char> data1(fileSize1);
    file1.read(data1.data(), fileSize1);
    file1.close();

    // Read the contents of the second binary file
    std::ifstream file2(file2Path, std::ios::binary | std::ios::ate);
    if (!file2.is_open()) {
        std::cerr << "Error opening file: " << file2Path << std::endl;
        return;
    }
    std::streamsize fileSize2 = file2.tellg();
    file2.seekg(0, std::ios::beg);
    std::vector<char> data2(fileSize2);
    file2.read(data2.data(), fileSize2);
    file2.close();

    // Delimiter to be added between the two files
    const char delimiter[] = {static_cast<char>(0x7F), static_cast<char>(0xFE)};
  // Assuming the delimiter is 011111111110 in binary

    // Combine the data and add the delimiter
    std::ofstream outputFile(outputPath, std::ios::binary);
    if (!outputFile.is_open()) {
        std::cerr << "Error opening file: " << outputPath << std::endl;
        return;
    }
    outputFile.write(data1.data(), fileSize1);
    outputFile.write(delimiter, sizeof(delimiter));
    outputFile.write(data2.data(), fileSize2);
    outputFile.close();
}

void separateBinaryFile(const std::string& inputPath, const std::string& output1Path, const std::string& output2Path, const char delimiter[], size_t delimiterSize) {
    // Read the contents of the combined binary file
    std::ifstream inputFile(inputPath, std::ios::binary | std::ios::ate);
    if (!inputFile.is_open()) {
        std::cerr << "Error opening file: " << inputPath << std::endl;
        return;
    }
    std::streamsize combinedFileSize = inputFile.tellg();
    inputFile.seekg(0, std::ios::beg);
    std::vector<char> combinedData(combinedFileSize);
    inputFile.read(combinedData.data(), combinedFileSize);
    inputFile.close();

    // Find the index of the delimiter in the combined data
    size_t delimiterIndex = 0;

    while (delimiterIndex + delimiterSize <= combinedData.size()) {
        if (memcmp(combinedData.data() + delimiterIndex, delimiter, delimiterSize) == 0) {
            // Delimiter found
            break;
        }
        delimiterIndex++;
    }

    // Check if the delimiter was found
    if (delimiterIndex + delimiterSize > combinedData.size()) {
        std::cerr << "Delimiter not found in the combined binary file." << std::endl;
        return;
    }

    // Separate the data into two parts
    std::vector<char> data1(combinedData.begin(), combinedData.begin() + delimiterIndex);
    std::vector<char> data2(combinedData.begin() + delimiterIndex + delimiterSize, combinedData.end());

    // Write the separated data to the output files
    std::ofstream output1File(output1Path, std::ios::binary);
    if (!output1File.is_open()) {
        std::cerr << "Error opening file: " << output1Path << std::endl;
        return;
    }
    output1File.write(data1.data(), data1.size());
    output1File.close();

    std::ofstream output2File(output2Path, std::ios::binary);
    if (!output2File.is_open()) {
        std::cerr << "Error opening file: " << output2Path << std::endl;
        return;
    }
    output2File.write(data2.data(), data2.size());
    output2File.close();
}

void encodeMapToBinaryFile(const std::map<char, std::string>& myMap, const std::string& filename) {
    // Open a binary file for writing
    std::ofstream outFileBin(filename + ".bin", std::ios::binary);

    // Write each key-value pair to the binary file
    for (const auto& entry : myMap) {
        // Write the char as binary data
        outFileBin.write(reinterpret_cast<const char*>(&entry.first), sizeof(entry.first));

        // Write the string length as binary data
        size_t len = entry.second.size();
        outFileBin.write(reinterpret_cast<const char*>(&len), sizeof(len));

        // Write the string as binary data
        outFileBin.write(entry.second.c_str(), len);
    }

    // Close the binary file
    outFileBin.close();
}


//decode map using bin file
std::map<char, std::string> decodeBinaryFileToMap(const std::string& filename) {
    std::map<char, std::string> myMap;

    // Open the binary file for reading
    std::ifstream inFileBin(filename, std::ios::binary);

    // Check if the file is open
    if (!inFileBin.is_open()) {
        std::cerr << "Error opening file: " << filename << "\n";
        return myMap; // Return an empty map on error
    }

    // Read each key-value pair from the binary file
    while (!inFileBin.eof()) {
        char key;
        size_t len;

        // Read the char from binary data
        inFileBin.read(reinterpret_cast<char*>(&key), sizeof(key));

        // Check for end-of-file before reading the length
        if (inFileBin.eof()) {
            break;
        }

        // Read the string length from binary data
        inFileBin.read(reinterpret_cast<char*>(&len), sizeof(len));

        // Read the string from binary data
        char buffer[len + 1];
        inFileBin.read(buffer, len);
        buffer[len] = '\0';

        myMap[key] = std::string(buffer);
    }

    // Close the binary file
    inFileBin.close();

    return myMap;
}


//Plag
template <typename T>
void reverseVector(std::vector<T>& vec) {
    size_t start = 0;
    size_t end = vec.size() - 1;

    while (start < end) {
        // Swap elements at the start and end indices
        std::swap(vec[start], vec[end]);

        // Move the indices towards the center
        ++start;
        --end;
    }
}


std::map<long, char> reverseMap(const std::map<char, long>& originalMap) {
    std::map<long, char> reversedMap;

    for (const auto& entry : originalMap) {
        reversedMap[entry.second] = entry.first;
    }

    return reversedMap;
}





std::map<char, std::string> generateHuffmanCodes(std::vector<long> &A, std::vector<char> &B)
{
    int n = A.size();

    std::map<char, std::string> codeWordLength;

	// Phase 1
    //r= root s= leaf t= next
	int root, leaf, next;
	for(leaf=0, root=0, next=0; next<n-1; next++) {
		int sum = 0;
		for(int i=0; i<2; i++) {
			if(leaf>=n || (root<next && A[root]<A[leaf])) {
				sum += A[root];
				A[root] = next;
				root++;
			}
			else {
				sum += A[leaf];
				if(leaf > next) {
					A[leaf] = 0;
				}
				leaf++;
			}
		}
		A[next] = sum;
	}
	
	//phase2
    int i = n; 
	int level_top = n - 2; //root
	int depth = 1;
	int j, k;
	int total_nodes_at_level = 2;
	while(i > 0) {
		for(k=level_top; k>0 && A[k-1]>=level_top; k--) {}

		int internal_nodes_at_level = level_top - k;
		int leaves_at_level = total_nodes_at_level - internal_nodes_at_level;
		for(j=0; j<leaves_at_level; j++) {
			A[--i] = depth;
		}

		total_nodes_at_level = internal_nodes_at_level * 2;
		level_top = k;
		depth++;
	}

    reverseVector(A);
    reverseVector(B);
    // assigning code lexigraphically
    std::vector<std::string> huffmanCodes;
    char shuniya = '0';
    // setting up length of first character
    std::string result = "";
    for (int i = 0; i < A[0]; i++)
    {
        result = result + shuniya;
    }
    huffmanCodes.push_back(result);


    i = 1;
    while (i < A.size()) {
        result = "";
        std::string temp = huffmanCodes[i - 1];
        temp = addBinary(temp, "1");
        int tempLength = temp.length();
        
        int j = 0;
        while (j < A[i] - tempLength) {
            temp = temp + shuniya;
            ++j;
        }
        
        result = temp;
        huffmanCodes.push_back(result);

        ++i;
    }
    for (int i = 0; i < B.size(); i++)
    {
        codeWordLength[B[i]] = huffmanCodes[i];
    }

    return codeWordLength;
}



//plag

void deflate(std::string filename){

    std::map<char, long> frequency;
    std::multimap<long, char> sortedOnBasisOfFrequency;
    std::vector<long> frequencyIncreasing;
    std::vector<char> charAccordingToFreq;
    std::map<char, std::string> charWithHuffmanCodes;

    std::ifstream inputFile(filename);

    if (inputFile.is_open())
    {
        
        
   

    char ch;

    while (inputFile.get(ch))
    {
        frequency[ch]++;
    }
    }
    else{
        std::cerr << "error opening file " << std::endl;
        return;

    }


    inputFile.close();

    std::ifstream decodeFile(filename);

    for (const auto &pair : frequency)
    {
        sortedOnBasisOfFrequency.insert(std::make_pair(pair.second, pair.first));
    }

   
    std::string strFilename(filename);
    for (auto it = sortedOnBasisOfFrequency.begin(); it != sortedOnBasisOfFrequency.end(); ++it)
    {
        frequencyIncreasing.push_back(it->first);
        charAccordingToFreq.push_back(it->second);
    }
    std::string filenameWithoutExtension = strFilename.substr(0, strFilename.find_last_of("."));

    charWithHuffmanCodes = generateHuffmanCodes(frequencyIncreasing, charAccordingToFreq);

    
    // std::ifstream decodeFile(filename);

    if (decodeFile.is_open())
    {
    
    char c;


    // std::ofstream output("HCoutput.txt", std::ios::out | std::ios::trunc);

    
    
    std::ofstream output(filenameWithoutExtension + ".bin", std::ios::binary);



    if (output.is_open())
    {
        char packedBits = 0, k;
        // Variable to keep track of the bit position in the packed byte
        int bitIndex = 0;
        // ofstream outputStream("exoutput.txt");
        while (decodeFile.get(c))
        {
            for(int i=0 ; i<charWithHuffmanCodes[c].size(); i++){
                k = charWithHuffmanCodes[c][i];


            
            packedBits |= (k == '1') ? (1 << bitIndex) : 0;

        // Move to the next bit position
            bitIndex++;

        // If we've filled a byte, write it to the file
            if (bitIndex == 8) {
                output.write(&packedBits, sizeof(char));

            // Reset the packedBits and bitIndex for the next byte
                packedBits = 0;
                bitIndex = 0;
            }

            }
            // outputStream << c;
        }

        if (bitIndex > 0) {
        output.write(&packedBits, sizeof(char));
    }
        // outputStream.close();

    }
    else
    {
        std::cerr << "Error opening the file for writing." << std::endl;
        return;
    }
    output.close();
    }
    else{
        std::cerr << "Error opening the file for writing." << std::endl;
        return;
    }
    decodeFile.close();

    
    encodeMapToBinaryFile(charWithHuffmanCodes, "encoded_data");
    //readBinaryFile("encoded_data.bin");
    combineBinaryFiles("encoded_data.bin", filenameWithoutExtension + ".bin", filenameWithoutExtension +"compress" + ".bin");
    //writeToBinaryFile("combined_file.txt");

}




void inflate(std::string filename, std::map<char, std::string> result, std::string opFileName){
    // Create a new map with reversed key-value pairs
    std::map<std::string, char> reverseValue;

    for (const auto &entry : result)
    {
        reverseValue[entry.second] = entry.first;
    }

    std::string currentCode;
    char c;
    std::ifstream heoutput(filename);
    std::string strFilename(opFileName);
    std::string filenameWithoutExtension = strFilename.substr(0, strFilename.find_last_of("."));
    std::string filePath = "clientDatabase/";
    std::ofstream hufmanoutput(filePath + opFileName);
    // std::ofstream hufmanoutput("HDoutput.txt");
    if (!hufmanoutput.is_open())
    {
        std::cout << "Error huffman output is not open.";
        return ;
    }
    while(heoutput.get(c))
    {
        currentCode += c;

        // Check if the current code matches a Huffman code
        if (reverseValue.count(currentCode))
        {
            // Append the corresponding character to the decoded data
            hufmanoutput << reverseValue[currentCode];

            // Reset the current code
            currentCode.clear();
        }
    }
    heoutput.close();
    hufmanoutput.close();
}
