#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>
#include <time.h>

#define MAX_STRING 256
#define B_PLUS_TREE_ORDER 5  // Order of B+ Tree
#define MIN_DOWN_PAYMENT_PERCENT 20.0
#define COMMISSION_RATE 0.02  // 2% commission

// File paths
#define CAR_DATA_FILE "car_data.dat"
#define SALESPERSON_DATA_FILE "salesperson_data.dat"
#define CUSTOMER_DATA_FILE "customer_data.dat"
#define SALES_DATA_FILE "sales_data.dat"
#define SHOWROOM_DATA_FILE "showroom_data.dat"

// Forward declarations
typedef struct BPlusTreeNode BPlusTreeNode;
typedef struct CarNode CarNode;
typedef struct SalesPersonNode SalesPersonNode;
typedef struct CustomerNode CustomerNode;

// Data structures
typedef struct Car {
    char VIN[MAX_STRING];  // Primary key
    char name[MAX_STRING];
    char color[MAX_STRING];
    double price;
    char fuelType[MAX_STRING];
    char bodyType[MAX_STRING];  // Hatchback, Sedan or SUV
    char showroomId[MAX_STRING];
    bool available;  // true if available, false if sold
    
    // For sold cars
    char customerId[MAX_STRING];
    char salesPersonId[MAX_STRING];
    char paymentType[MAX_STRING];  // Cash/Loan
    
    // For EMI payments
    int emiMonths;
    double downPayment;
    double emiRate;
} Car;

typedef struct SalesPerson {
    char id[MAX_STRING];  // Primary key
    char name[MAX_STRING];
    char showroomId[MAX_STRING];
    double target;  // in lakhs rupees
    double achieved;  // in lakhs rupees
    double commission;  // 2% of sales achieved
} SalesPerson;

typedef struct Customer {
    char id[MAX_STRING];  // Primary key
    char name[MAX_STRING];
    char mobileNo[MAX_STRING];
    char address[MAX_STRING];
    int numPurchasedCars;
    char purchasedCars[10][MAX_STRING];  // Assuming max 10 cars per customer - Array of VINs
} Customer;

typedef struct Showroom {
    char id[MAX_STRING];  // Primary key
    char name[MAX_STRING];
    char manufacturer[MAX_STRING];
} Showroom;

// B+ Tree Structures
struct BPlusTreeNode {
    bool isLeaf;
    int numKeys;
    char keys[B_PLUS_TREE_ORDER - 1][MAX_STRING];  // Using VIN as key
    BPlusTreeNode* next; // For leaf nodes to link to the next leaf
    BPlusTreeNode* parent; // Added parent pointer for easier navigation
    
    // Union for pointer types
    union {
        BPlusTreeNode* children[B_PLUS_TREE_ORDER];
        void* dataPointers[B_PLUS_TREE_ORDER - 1];
    };
};

// Specific node types for our data
struct CarNode {
    Car car;
    struct CarNode* next;
};

struct SalesPersonNode {
    SalesPerson salesPerson;
    struct SalesPersonNode* next;
};

struct CustomerNode {
    Customer customer;
    struct CustomerNode* next;
};

// Global trees
BPlusTreeNode* carVinTree = NULL;  // Main car tree by VIN
BPlusTreeNode** showroomCarTrees = NULL;  // Array of trees, one per showroom
BPlusTreeNode* salesPersonTree = NULL;
BPlusTreeNode* customerTree = NULL;
BPlusTreeNode* carSalesTree = NULL;  // For tracking sales

// Global linked lists for data
CarNode* carList = NULL;
SalesPersonNode* salesPersonList = NULL;
CustomerNode* customerList = NULL;

// Number of showrooms
int numShowrooms = 0;
Showroom* showrooms = NULL;

// Function prototypes
// B+ Tree operations
BPlusTreeNode* createNode(bool isLeaf);
BPlusTreeNode* findLeaf(BPlusTreeNode* root, const char* key);
void* search(BPlusTreeNode* root, const char* key);
void insertIntoTree(BPlusTreeNode** rootPtr, const char* key, void* value);
void splitLeaf(BPlusTreeNode* leaf, BPlusTreeNode** rootPtr);
void splitNonLeaf(BPlusTreeNode* node, BPlusTreeNode** rootPtr);
void insertIntoParent(BPlusTreeNode* left, BPlusTreeNode* right, const char* key, BPlusTreeNode** rootPtr);

// File operations
void saveCarToFile(Car* car);
void saveSalesPersonToFile(SalesPerson* salesPerson);
void saveCustomerToFile(Customer* customer);
void loadDataFromFiles();
void ensureFilesExist();

// Tree initialization
void initializeTrees();

// Utility functions
int compareStrings(const char* str1, const char* str2);
char* createNewId(const char* prefix);

// Data manipulation functions
void addCar(Car* car);
void addSalesPerson(SalesPerson* salesPerson);
void addCustomer(Customer* customer);
void sellCar(const char* VIN, const char* customerId, const char* salesPersonId, const char* paymentType, int emiMonths, double downPayment);

// Required functions from problem statement
void mergeShowrooms(const char* outputFileName);
void addNewSalesPerson(SalesPerson* salesPerson);
char* findMostPopularCar();
SalesPerson* findMostSuccessfulSalesPerson();
void sellCarToCustomer(const char* VIN, const char* customerId, const char* salesPersonId, const char* paymentType, int emiMonths, double downPayment);
void predictNextMonthSales();
void displayCarInfo(const char* VIN);
void findSalesPersonByTargetRange(double minSales, double maxSales);
void listCustomersByEmiRange(int minMonths, int maxMonths);
void freeMemory();

// Implementation of core functions
BPlusTreeNode* createNode(bool isLeaf) {
    BPlusTreeNode* newNode = (BPlusTreeNode*)malloc(sizeof(BPlusTreeNode));
    if (!newNode) {
        fprintf(stderr, "Memory allocation failed\n");
        exit(1);
    }
    
    newNode->isLeaf = isLeaf;
    newNode->numKeys = 0;
    newNode->next = NULL;
    newNode->parent = NULL; // Initialize parent to NULL
    
    // Initialize keys and pointers
    for (int i = 0; i < B_PLUS_TREE_ORDER - 1; i++) {
        newNode->keys[i][0] = '\0';
        if (isLeaf) {
            newNode->dataPointers[i] = NULL;
        }
    }
    
    if (!isLeaf) {
        for (int i = 0; i < B_PLUS_TREE_ORDER; i++) {
            newNode->children[i] = NULL;
        }
    }
    
    return newNode;
}

BPlusTreeNode* findLeaf(BPlusTreeNode* root, const char* key) {
    if (!root) return NULL;
    
    BPlusTreeNode* current = root;
    while (!current->isLeaf) {
        int i = 0;
        while (i < current->numKeys && compareStrings(key, current->keys[i]) >= 0) {
            i++;
        }
        current = current->children[i];
    }
    
    return current;
}

void* search(BPlusTreeNode* root, const char* key) {
    if (!root) return NULL;
    
    BPlusTreeNode* leaf = findLeaf(root, key);
    if (!leaf) return NULL;
    
    for (int i = 0; i < leaf->numKeys; i++) {
        if (strcmp(leaf->keys[i], key) == 0) {
            return leaf->dataPointers[i];
        }
    }
    
    return NULL;
}

void insertIntoParent(BPlusTreeNode* left, BPlusTreeNode* right, const char* key, BPlusTreeNode** rootPtr) {
    if (!left->parent) {
        // Create a new root
        BPlusTreeNode* newRoot = createNode(false);
        strcpy(newRoot->keys[0], key);
        newRoot->children[0] = left;
        newRoot->children[1] = right;
        newRoot->numKeys = 1;
        
        left->parent = newRoot;
        right->parent = newRoot;
        
        *rootPtr = newRoot;
        return;
    }
    
    // Find position to insert in parent
    BPlusTreeNode* parent = left->parent;
    int i = 0;
    while (i < parent->numKeys && parent->children[i] != left) {
        i++;
    }
    
    // Shift keys and pointers to make room
    for (int j = parent->numKeys; j > i; j--) {
        strcpy(parent->keys[j], parent->keys[j-1]);
        parent->children[j+1] = parent->children[j];
    }
    
    strcpy(parent->keys[i], key);
    parent->children[i+1] = right;
    parent->numKeys++;
    right->parent = parent;
    
    // Check if parent needs splitting
    if (parent->numKeys == B_PLUS_TREE_ORDER - 1) {
        splitNonLeaf(parent, rootPtr);
    }
}

void splitLeaf(BPlusTreeNode* leaf, BPlusTreeNode** rootPtr) {
    // Create a new leaf node
    BPlusTreeNode* newLeaf = createNode(true);
    int mid = (B_PLUS_TREE_ORDER - 1) / 2;
    
    // Move half the keys to the new leaf
    for (int i = mid; i < B_PLUS_TREE_ORDER - 1; i++) {
        strcpy(newLeaf->keys[i - mid], leaf->keys[i]);
        newLeaf->dataPointers[i - mid] = leaf->dataPointers[i];
        leaf->keys[i][0] = '\0';
        leaf->dataPointers[i] = NULL;
    }
    
    newLeaf->numKeys = leaf->numKeys - mid;
    leaf->numKeys = mid;
    
    // Set the next pointers for leaf nodes
    newLeaf->next = leaf->next;
    leaf->next = newLeaf;
    
    // Insert into parent
    char keyUp[MAX_STRING];
    strcpy(keyUp, newLeaf->keys[0]);
    insertIntoParent(leaf, newLeaf, keyUp, rootPtr);
}

void splitNonLeaf(BPlusTreeNode* node, BPlusTreeNode** rootPtr) {
    // Create a new non-leaf node
    BPlusTreeNode* newNode = createNode(false);
    int mid = (B_PLUS_TREE_ORDER - 1) / 2;
    
    // Key that will move up to the parent
    char keyUp[MAX_STRING];
    strcpy(keyUp, node->keys[mid]);
    
    // Move keys and children to the new node
    for (int i = mid + 1; i < B_PLUS_TREE_ORDER - 1; i++) {
        strcpy(newNode->keys[i - (mid + 1)], node->keys[i]);
        node->keys[i][0] = '\0';
    }
    
    for (int i = mid + 1; i < B_PLUS_TREE_ORDER; i++) {
        newNode->children[i - (mid + 1)] = node->children[i];
        if (node->children[i]) {
            node->children[i]->parent = newNode;
        }
        node->children[i] = NULL;
    }
    
    // Update key counts
    newNode->numKeys = node->numKeys - mid - 1;
    node->numKeys = mid;
    
    // Clear the key that moves up
    node->keys[mid][0] = '\0';
    
    // Insert into parent
    insertIntoParent(node, newNode, keyUp, rootPtr);
}

void insertIntoTree(BPlusTreeNode** rootPtr, const char* key, void* value) {
    // If tree is empty, create a new root
    if (!(*rootPtr)) {
        *rootPtr = createNode(true);
        strcpy((*rootPtr)->keys[0], key);
        (*rootPtr)->dataPointers[0] = value;
        (*rootPtr)->numKeys = 1;
        return;
    }
    
    // Find the leaf node where the key should be inserted
    BPlusTreeNode* leaf = findLeaf(*rootPtr, key);
    
    // Check if key already exists
    for (int i = 0; i < leaf->numKeys; i++) {
        if (strcmp(leaf->keys[i], key) == 0) {
            leaf->dataPointers[i] = value;  // Update value
            return;
        }
    }
    
    // Find position to insert
    int i = leaf->numKeys - 1;
    while (i >= 0 && compareStrings(key, leaf->keys[i]) < 0) {
        strcpy(leaf->keys[i + 1], leaf->keys[i]);
        leaf->dataPointers[i + 1] = leaf->dataPointers[i];
        i--;
    }
    
    strcpy(leaf->keys[i + 1], key);
    leaf->dataPointers[i + 1] = value;
    leaf->numKeys++;
    
    // Check if node needs to be split
    if (leaf->numKeys == B_PLUS_TREE_ORDER - 1) {
        splitLeaf(leaf, rootPtr);
    }
}

// Utility functions
int compareStrings(const char* str1, const char* str2) {
    return strcmp(str1, str2);
}

char* createNewId(const char* prefix) {
    static int counter = 1;
    char* id = (char*)malloc(MAX_STRING);
    if (!id) {
        fprintf(stderr, "Memory allocation failed\n");
        exit(1);
    }
    sprintf(id, "%s%d", prefix, counter++);
    return id;
}

// Ensure all required files exist
void ensureFilesExist() {
    FILE* file;
    
    // Check car data file
    file = fopen(CAR_DATA_FILE, "r");
    if (!file) {
        file = fopen(CAR_DATA_FILE, "w");
        if (!file) {
            fprintf(stderr, "Failed to create car data file\n");
            exit(1);
        }
    }
    fclose(file);
    
    // Check salesperson data file
    file = fopen(SALESPERSON_DATA_FILE, "r");
    if (!file) {
        file = fopen(SALESPERSON_DATA_FILE, "w");
        if (!file) {
            fprintf(stderr, "Failed to create salesperson data file\n");
            exit(1);
        }
    }
    fclose(file);
    
    // Check customer data file
    file = fopen(CUSTOMER_DATA_FILE, "r");
    if (!file) {
        file = fopen(CUSTOMER_DATA_FILE, "w");
        if (!file) {
            fprintf(stderr, "Failed to create customer data file\n");
            exit(1);
        }
    }
    fclose(file);
    
    // Check sales data file
    file = fopen(SALES_DATA_FILE, "r");
    if (!file) {
        file = fopen(SALES_DATA_FILE, "w");
        if (!file) {
            fprintf(stderr, "Failed to create sales data file\n");
            exit(1);
        }
    }
    fclose(file);
    
    // Check showroom data file
    file = fopen(SHOWROOM_DATA_FILE, "r");
    if (!file) {
        file = fopen(SHOWROOM_DATA_FILE, "w");
        if (!file) {
            fprintf(stderr, "Failed to create showroom data file\n");
            exit(1);
        }
        
        // Create a default showroom if file was just created
        fprintf(file, "SHW1,Main Showroom,Default\n");
    }
    fclose(file);
}

// File operations
void saveCarToFile(Car* car) {
    FILE* file = fopen(CAR_DATA_FILE, "a");
    if (!file) {
        fprintf(stderr, "Failed to open car data file\n");
        return;
    }
    
    fprintf(file, "%s,%s,%s,%.2f,%s,%s,%s,%d", 
            car->VIN, car->name, car->color, car->price, 
            car->fuelType, car->bodyType, car->showroomId, car->available);
    
    if (!car->available) {
        fprintf(file, ",%s,%s,%s", car->customerId, car->salesPersonId, car->paymentType);
        if (strcmp(car->paymentType, "Loan") == 0) {
            fprintf(file, ",%d,%.2f,%.2f", car->emiMonths, car->downPayment, car->emiRate);
        }
    }
    
    fprintf(file, "\n");
    fclose(file);
}

void saveSalesPersonToFile(SalesPerson* salesPerson) {
    FILE* file = fopen(SALESPERSON_DATA_FILE, "a");
    if (!file) {
        fprintf(stderr, "Failed to open salesperson data file\n");
        return;
    }
    
    fprintf(file, "%s,%s,%s,%.2f,%.2f,%.2f\n", 
            salesPerson->id, salesPerson->name, salesPerson->showroomId, 
            salesPerson->target, salesPerson->achieved, salesPerson->commission);
    
    fclose(file);
}

void saveCustomerToFile(Customer* customer) {
    FILE* file = fopen(CUSTOMER_DATA_FILE, "a");
    if (!file) {
        fprintf(stderr, "Failed to open customer data file\n");
        return;
    }
    
    fprintf(file, "%s,%s,%s,%s", 
            customer->id, customer->name, customer->mobileNo, customer->address);
    
    if (customer->numPurchasedCars > 0) {
        fprintf(file, ",%d", customer->numPurchasedCars);
        for (int i = 0; i < customer->numPurchasedCars; i++) {
            fprintf(file, ",%s", customer->purchasedCars[i]);
        }
    }
    
    fprintf(file, "\n");
    fclose(file);
}

// Required functions from problem statement
void mergeShowrooms(const char* outputFileName) {
    // Define the three input showroom files
    const char* inputFiles[] = {
        "showroom1.dat",
        "showroom2.dat",
        "showroom3.dat"
    };
    const int numInputFiles = 3;

    // Open all input files
    FILE* inputFilePtrs[3];
    for (int i = 0; i < numInputFiles; i++) {
        inputFilePtrs[i] = fopen(inputFiles[i], "r");
        if (!inputFilePtrs[i]) {
            fprintf(stderr, "Failed to open input file: %s\n", inputFiles[i]);
            // Close any files that were successfully opened
            for (int j = 0; j < i; j++) {
                fclose(inputFilePtrs[j]);
            }
            return;
        }
    }

    // Open the output file
    FILE* outputFile = fopen(outputFileName, "w");
    if (!outputFile) {
        fprintf(stderr, "Failed to create output file\n");
        for (int i = 0; i < numInputFiles; i++) {
            fclose(inputFilePtrs[i]);
        }
        return;
    }

    // Header for the output file
    fprintf(outputFile, "VIN,CarName,Color,Price,FuelType,BodyType,ShowroomID,Available\n");

    // We'll use a simple merge approach by reading one line from each file
    // and always writing the smallest VIN to the output file
    char currentLines[3][1024];
    char currentVINs[3][MAX_STRING];
    bool fileFinished[3] = {false};
    int filesRemaining = numInputFiles;

    // Initialize by reading first line from each file
    for (int i = 0; i < numInputFiles; i++) {
        if (fgets(currentLines[i], sizeof(currentLines[i]), inputFilePtrs[i])) {
            // Remove newline character if present
            currentLines[i][strcspn(currentLines[i], "\r\n")] = '\0';
            
            // Extract VIN from the line (first field)
            strcpy(currentVINs[i], strtok(currentLines[i], ","));
        } else {
            fileFinished[i] = true;
            filesRemaining--;
        }
    }

    // Merge process
    while (filesRemaining > 0) {
        // Find the file with the smallest current VIN
        int minIndex = -1;
        for (int i = 0; i < numInputFiles; i++) {
            if (!fileFinished[i]) {
                if (minIndex == -1 || compareStrings(currentVINs[i], currentVINs[minIndex]) < 0) {
                    minIndex = i;
                }
            }
        }

        if (minIndex == -1) {
            break;  // Shouldn't happen if filesRemaining > 0
        }

        // Write the line to output file
        fprintf(outputFile, "%s\n", currentLines[minIndex]);

        // Read next line from this file
        if (fgets(currentLines[minIndex], sizeof(currentLines[minIndex]), inputFilePtrs[minIndex])) {
            // Remove newline character if present
            currentLines[minIndex][strcspn(currentLines[minIndex], "\r\n")] = '\0';
            
            // Extract VIN from the new line
            strcpy(currentVINs[minIndex], strtok(currentLines[minIndex], ","));
        } else {
            fileFinished[minIndex] = true;
            filesRemaining--;
        }
    }

    // Close all files
    for (int i = 0; i < numInputFiles; i++) {
        fclose(inputFilePtrs[i]);
    }
    fclose(outputFile);

    printf("Successfully merged showroom data to %s, sorted by VIN\n", outputFileName);
}
void addNewSalesPerson(SalesPerson* salesPerson) {
    // Create a new sales person node
    SalesPersonNode* newNode = (SalesPersonNode*)malloc(sizeof(SalesPersonNode));
    if (!newNode) {
        fprintf(stderr, "Memory allocation failed\n");
        return;
    }
    
    // Generate a new ID if not provided
    if (strlen(salesPerson->id) == 0) {
        char* id = createNewId("SP");
        strcpy(salesPerson->id, id);
        free(id);
    }
    
    // Copy the sales person data
    memcpy(&newNode->salesPerson, salesPerson, sizeof(SalesPerson));
    
    // Insert into linked list
    newNode->next = salesPersonList;
    salesPersonList = newNode;
    
    // Insert into B+ tree
    insertIntoTree(&salesPersonTree, salesPerson->id, (void*)newNode);
    
    // Save to file
    saveSalesPersonToFile(salesPerson);
    
    printf("Sales person added with ID: %s\n", salesPerson->id);
}

char* findMostPopularCar() {
    // Create a map to count occurrences of each car model
    struct {
        char model[MAX_STRING];
        int count;
    } modelCounts[1000];  // Assuming max 1000 different models
    int numModels = 0;
    
    // Traverse all cars and count models
    CarNode* current = carList;
    while (current) {
        bool found = false;
        for (int i = 0; i < numModels; i++) {
            if (strcmp(modelCounts[i].model, current->car.name) == 0) {
                modelCounts[i].count++;
                found = true;
                break;
            }
        }
        
        if (!found && numModels < 1000) {
            strcpy(modelCounts[numModels].model, current->car.name);
            modelCounts[numModels].count = 1;
            numModels++;
        }
        
        current = current->next;
    }
    
    // Find the most popular model
    int maxCount = 0;
    char* mostPopular = (char*)malloc(MAX_STRING);
    if (!mostPopular) {
        fprintf(stderr, "Memory allocation failed\n");
        exit(1);
    }
    mostPopular[0] = '\0';
    
    for (int i = 0; i < numModels; i++) {
        if (modelCounts[i].count > maxCount) {
            maxCount = modelCounts[i].count;
            strcpy(mostPopular, modelCounts[i].model);
        }
    }
    
    return mostPopular;
}

SalesPerson* findMostSuccessfulSalesPerson() {
    SalesPersonNode* current = salesPersonList;
    SalesPerson* mostSuccessful = NULL;
    double maxAchieved = 0;
    
    while (current) {
        if (current->salesPerson.achieved > maxAchieved) {
            maxAchieved = current->salesPerson.achieved;
            mostSuccessful = &current->salesPerson;
        }
        current = current->next;
    }
    
    // Calculate incentive for the most successful salesperson
    if (mostSuccessful) {
        mostSuccessful->commission += (mostSuccessful->achieved * 0.01); // 1% incentive
    }
    
    return mostSuccessful;
}

void sellCarToCustomer(const char* VIN, const char* customerId, const char* salesPersonId, const char* paymentType, int emiMonths, double downPayment) {
    // Find the car
    CarNode* carNode = (CarNode*)search(carVinTree, VIN);
    if (!carNode) {
        printf("Car not found with VIN: %s\n", VIN);
        return;
    }
    
    if (!carNode->car.available) {
        printf("Car with VIN %s is already sold\n", VIN);
        return;
    }
    
    // Find the customer
    CustomerNode* customerNode = (CustomerNode*)search(customerTree, customerId);
    if (!customerNode) {
        printf("Customer not found with ID: %s\n", customerId);
        return;
    }
    
    // Find the sales person
    SalesPersonNode* salesPersonNode = (SalesPersonNode*)search(salesPersonTree, salesPersonId);
    if (!salesPersonNode) {
        printf("Sales person not found with ID: %s\n", salesPersonId);
        return;
    }
    
    // Check if down payment is sufficient for loan
    if (strcmp(paymentType, "Loan") == 0) {
        double minDownPayment = (carNode->car.price * MIN_DOWN_PAYMENT_PERCENT) / 100.0;
        if (downPayment < minDownPayment) {
            printf("Down payment must be at least %.2f (%.2f%% of price)\n", 
                   minDownPayment, MIN_DOWN_PAYMENT_PERCENT);
            return;
        }
    }
    
    // Update car data
    carNode->car.available = false;
    strcpy(carNode->car.customerId, customerId);
    strcpy(carNode->car.salesPersonId, salesPersonId);
    strcpy(carNode->car.paymentType, paymentType);
    
    if (strcmp(paymentType, "Loan") == 0) {
        carNode->car.emiMonths = emiMonths;
        carNode->car.downPayment = downPayment;
        
        // Set EMI rate based on months
        if (emiMonths <= 36) {
            carNode->car.emiRate = 8.50;
        } else if (emiMonths <= 60) {
            carNode->car.emiRate = 8.75;
        } else {
            carNode->car.emiRate = 9.00;
        }
    }
    
    // Update customer data
    if (customerNode->customer.numPurchasedCars < 10) {
        strcpy(customerNode->customer.purchasedCars[customerNode->customer.numPurchasedCars], VIN);
        customerNode->customer.numPurchasedCars++;
    }
    
    // Update sales person data
    double carPriceInLakhs = carNode->car.price / 100000.0;  // Convert to lakhs
    salesPersonNode->salesPerson.achieved += carPriceInLakhs;
    salesPersonNode->salesPerson.commission = salesPersonNode->salesPerson.achieved * COMMISSION_RATE;
    
    // Update files - rewrite all data
    FILE* carFile = fopen(CAR_DATA_FILE, "w");
    if (carFile) {
        CarNode* current = carList;
        while (current) {
            fprintf(carFile, "%s,%s,%s,%.2f,%s,%s,%s,%d", 
                current->car.VIN, current->car.name, current->car.color, current->car.price, 
                current->car.fuelType, current->car.bodyType, current->car.showroomId, current->car.available);
            
            if (!current->car.available) {
                fprintf(carFile, ",%s,%s,%s", current->car.customerId, current->car.salesPersonId, current->car.paymentType);
                if (strcmp(current->car.paymentType, "Loan") == 0) {
                    fprintf(carFile, ",%d,%.2f,%.2f", current->car.emiMonths, current->car.downPayment, current->car.emiRate);
                }
            }
            
            fprintf(carFile, "\n");
            current = current->next;
        }
        fclose(carFile);
    }
    
    FILE* customerFile = fopen(CUSTOMER_DATA_FILE, "w");
    if (customerFile) {
        CustomerNode* current = customerList;
        while (current) {
            fprintf(customerFile, "%s,%s,%s,%s", 
                current->customer.id, current->customer.name, current->customer.mobileNo, current->customer.address);
            
            if (current->customer.numPurchasedCars > 0) {
                fprintf(customerFile, ",%d", current->customer.numPurchasedCars);
                for (int i = 0; i < current->customer.numPurchasedCars; i++) {
                    fprintf(customerFile, ",%s", current->customer.purchasedCars[i]);
                }
            }
            
            fprintf(customerFile, "\n");
            current = current->next;
        }
        fclose(customerFile);
    }
    
    FILE* salesPersonFile = fopen(SALESPERSON_DATA_FILE, "w");
    if (salesPersonFile) {
        SalesPersonNode* current = salesPersonList;
        while (current) {
            fprintf(salesPersonFile, "%s,%s,%s,%.2f,%.2f,%.2f\n", 
                current->salesPerson.id, current->salesPerson.name, current->salesPerson.showroomId, 
                current->salesPerson.target, current->salesPerson.achieved, current->salesPerson.commission);
            
            current = current->next;
        }
        fclose(salesPersonFile);
    }
    
    printf("Car with VIN %s sold successfully to customer %s\n", VIN, customerNode->customer.name);
}

void predictNextMonthSales() {
    // Simple prediction based on previous month sales
    // In a real implementation, this would use more sophisticated techniques
    double totalSales = 0;
    int count = 0;
    
    SalesPersonNode* current = salesPersonList;
    while (current) {
        totalSales += current->salesPerson.achieved;
        count++;
        current = current->next;
    }
    
    if (count > 0) {
        double avgSales = totalSales / count;
        printf("Predicted next month sales: %.2f lakhs\n", avgSales * 1.05);  // 5% growth assumption
    } else {
        printf("No sales data available for prediction\n");
    }
}

void displayCarInfo(const char* VIN) {
    CarNode* carNode = (CarNode*)search(carVinTree, VIN);
    if (!carNode) {
        printf("Car not found with VIN: %s\n", VIN);
        return;
    }
    
    printf("\n=================== Car Details ===================\n");
    printf("VIN: %s\n", carNode->car.VIN);
    printf("Name: %s\n", carNode->car.name);
    printf("Color: %s\n", carNode->car.color);
    printf("Price: %.2f\n", carNode->car.price);
    printf("Fuel Type: %s\n", carNode->car.fuelType);
    printf("Body Type: %s\n", carNode->car.bodyType);
    printf("Showroom ID: %s\n", carNode->car.showroomId);
    printf("Available: %s\n", carNode->car.available ? "Yes" : "No");
    
    if (!carNode->car.available) {
        printf("\n----------------- Sale Details -----------------\n");
        printf("Customer ID: %s\n", carNode->car.customerId);
        printf("Sales Person ID: %s\n", carNode->car.salesPersonId);
        printf("Payment Type: %s\n", carNode->car.paymentType);
        
        if (strcmp(carNode->car.paymentType, "Loan") == 0) {
            printf("EMI Months: %d\n", carNode->car.emiMonths);
            printf("Down Payment: %.2f\n", carNode->car.downPayment);
            printf("EMI Rate: %.2f%%\n", carNode->car.emiRate);
            
            // Calculate EMI amount
            double principal = carNode->car.price - carNode->car.downPayment;
            double monthlyRate = carNode->car.emiRate / (12 * 100);
            double emiAmount = principal * monthlyRate * pow(1 + monthlyRate, carNode->car.emiMonths) / 
                              (pow(1 + monthlyRate, carNode->car.emiMonths) - 1);
            
            printf("Monthly EMI: %.2f\n", emiAmount);
        }
    }
    
    printf("==================================================\n");
}

void findSalesPersonByTargetRange(double minSales, double maxSales) {
    printf("\n========== Sales Persons in Target Range %.2f - %.2f ==========\n", minSales, maxSales);
    int count = 0;
    
    SalesPersonNode* current = salesPersonList;
    while (current) {
        if (current->salesPerson.achieved >= minSales && current->salesPerson.achieved <= maxSales) {
            printf("ID: %s, Name: %s, Achieved: %.2f lakhs\n", 
                   current->salesPerson.id, current->salesPerson.name, current->salesPerson.achieved);
            count++;
        }
        current = current->next;
    }
    
    if (count == 0) {
        printf("No sales persons found in the given range\n");
    } else {
        printf("Total: %d sales persons\n", count);
    }
    printf("========================================================\n");
}

void listCustomersByEmiRange(int minMonths, int maxMonths) {
    printf("\n========== Customers with EMI Range %d - %d months ==========\n", minMonths, maxMonths);
    int count = 0;
    
    // Find all sold cars with EMI in the given range and their customers
    CarNode* current = carList;
    while (current) {
        if (!current->car.available && 
            strcmp(current->car.paymentType, "Loan") == 0 &&
            current->car.emiMonths > minMonths && 
            current->car.emiMonths < maxMonths) {
            
            // Find customer details
            CustomerNode* custNode = (CustomerNode*)search(customerTree, current->car.customerId);
            if (custNode) {
                printf("Customer Name: %s, Car: %s, EMI Months: %d\n", 
                       custNode->customer.name, current->car.name, current->car.emiMonths);
                count++;
            }
        }
        current = current->next;
    }
    
    if (count == 0) {
        printf("No customers found with EMI in the given range\n");
    } else {
        printf("Total: %d customers\n", count);
    }
    printf("=========================================================\n");
}

void freeMemory() {
    // Free car list
    CarNode* currentCar = carList;
    while (currentCar) {
        CarNode* temp = currentCar;
        currentCar = currentCar->next;
        free(temp);
    }
    
    // Free sales person list
    SalesPersonNode* currentSP = salesPersonList;
    while (currentSP) {
        SalesPersonNode* temp = currentSP;
        currentSP = currentSP->next;
        free(temp);
    }
    
    // Free customer list
    CustomerNode* currentCust = customerList;
    while (currentCust) {
        CustomerNode* temp = currentCust;
        currentCust = currentCust->next;
        free(temp);
    }
    
    // Free showrooms array
    if (showrooms) {
        free(showrooms);
    }
    
    // Free B+ Trees (recursive helper function would be needed here)
    // This is a simplified version - a complete implementation would
    // recursively free all nodes in the trees
}

void loadDataFromFiles() {
    FILE* file;
    char line[1024];
    
    // Load showrooms
    file = fopen(SHOWROOM_DATA_FILE, "r");
    if (file) {
        // Count showrooms first
        numShowrooms = 0;
        while (fgets(line, sizeof(line), file)) {
            numShowrooms++;
        }
        
        // Allocate memory for showrooms
        showrooms = (Showroom*)malloc(numShowrooms * sizeof(Showroom));
        if (!showrooms) {
            fprintf(stderr, "Memory allocation failed\n");
            fclose(file);
            return;
        }
        
        // Reset file pointer and read data
        rewind(file);
        int idx = 0;
        while (fgets(line, sizeof(line), file) && idx < numShowrooms) {
            line[strcspn(line, "\r\n")] = 0;  // Remove newline
            
            char* token = strtok(line, ",");
            if (token) strcpy(showrooms[idx].id, token);
            
            token = strtok(NULL, ",");
            if (token) strcpy(showrooms[idx].name, token);
            
            token = strtok(NULL, ",");
            if (token) strcpy(showrooms[idx].manufacturer, token);
            
            idx++;
        }
        fclose(file);
        
        // Initialize showroom-specific car trees
        showroomCarTrees = (BPlusTreeNode**)malloc(numShowrooms * sizeof(BPlusTreeNode*));
        if (showroomCarTrees) {
            for (int i = 0; i < numShowrooms; i++) {
                showroomCarTrees[i] = NULL;
            }
        }
    }
    
    // Load cars
    file = fopen(CAR_DATA_FILE, "r");
    if (file) {
        while (fgets(line, sizeof(line), file)) {
            line[strcspn(line, "\r\n")] = 0;  // Remove newline
            
            Car car;
            memset(&car, 0, sizeof(Car));
            
            char* token = strtok(line, ",");
            if (token) strcpy(car.VIN, token);
            
            token = strtok(NULL, ",");
            if (token) strcpy(car.name, token);
            
            token = strtok(NULL, ",");
            if (token) strcpy(car.color, token);
            
            token = strtok(NULL, ",");
            if (token) car.price = atof(token);
            
            token = strtok(NULL, ",");
            if (token) strcpy(car.fuelType, token);
            
            token = strtok(NULL, ",");
            if (token) strcpy(car.bodyType, token);
            
            token = strtok(NULL, ",");
            if (token) strcpy(car.showroomId, token);
            
            token = strtok(NULL, ",");
            if (token) car.available = atoi(token);
            
            if (!car.available) {
                token = strtok(NULL, ",");
                if (token) strcpy(car.customerId, token);
                
                token = strtok(NULL, ",");
                if (token) strcpy(car.salesPersonId, token);
                
                token = strtok(NULL, ",");
                if (token) strcpy(car.paymentType, token);
                
                if (strcmp(car.paymentType, "Loan") == 0) {
                    token = strtok(NULL, ",");
                    if (token) car.emiMonths = atoi(token);
                    
                    token = strtok(NULL, ",");
                    if (token) car.downPayment = atof(token);
                    
                    token = strtok(NULL, ",");
                    if (token) car.emiRate = atof(token);
                }
            }
            
            // Add car to list and tree
            CarNode* newNode = (CarNode*)malloc(sizeof(CarNode));
            if (newNode) {
                memcpy(&newNode->car, &car, sizeof(Car));
                newNode->next = carList;
                carList = newNode;
                
                // Add to main car tree
                insertIntoTree(&carVinTree, car.VIN, (void*)newNode);
                
                // Add to showroom-specific tree
                for (int i = 0; i < numShowrooms; i++) {
                    if (strcmp(showrooms[i].id, car.showroomId) == 0) {
                        insertIntoTree(&showroomCarTrees[i], car.VIN, (void*)newNode);
                        break;
                    }
                }
            }
        }
        fclose(file);
    }
    
    // Load salespeople
    file = fopen(SALESPERSON_DATA_FILE, "r");
    if (file) {
        while (fgets(line, sizeof(line), file)) {
            line[strcspn(line, "\r\n")] = 0;  // Remove newline
            
            SalesPerson sp;
            memset(&sp, 0, sizeof(SalesPerson));
            
            char* token = strtok(line, ",");
            if (token) strcpy(sp.id, token);
            
            token = strtok(NULL, ",");
            if (token) strcpy(sp.name, token);
            
            token = strtok(NULL, ",");
            if (token) strcpy(sp.showroomId, token);
            
            token = strtok(NULL, ",");
            if (token) sp.target = atof(token);
            
            token = strtok(NULL, ",");
            if (token) sp.achieved = atof(token);
            
            token = strtok(NULL, ",");
            if (token) sp.commission = atof(token);
            
            // Add salesperson to list and tree
            SalesPersonNode* newNode = (SalesPersonNode*)malloc(sizeof(SalesPersonNode));
            if (newNode) {
                memcpy(&newNode->salesPerson, &sp, sizeof(SalesPerson));
                newNode->next = salesPersonList;
                salesPersonList = newNode;
                
                insertIntoTree(&salesPersonTree, sp.id, (void*)newNode);
            }
        }
        fclose(file);
    }
    
    // Load customers
    file = fopen(CUSTOMER_DATA_FILE, "r");
    if (file) {
        while (fgets(line, sizeof(line), file)) {
            line[strcspn(line, "\r\n")] = 0;  // Remove newline
            
            Customer cust;
            memset(&cust, 0, sizeof(Customer));
            
            char* token = strtok(line, ",");
            if (token) strcpy(cust.id, token);
            
            token = strtok(NULL, ",");
            if (token) strcpy(cust.name, token);
            
            token = strtok(NULL, ",");
            if (token) strcpy(cust.mobileNo, token);
            
            token = strtok(NULL, ",");
            if (token) strcpy(cust.address, token);
            
            token = strtok(NULL, ",");
            if (token) cust.numPurchasedCars = atoi(token);
            
            for (int i = 0; i < cust.numPurchasedCars && i < 10; i++) {
                token = strtok(NULL, ",");
                if (token) strcpy(cust.purchasedCars[i], token);
            }
            
            // Add customer to list and tree
            CustomerNode* newNode = (CustomerNode*)malloc(sizeof(CustomerNode));
            if (newNode) {
                memcpy(&newNode->customer, &cust, sizeof(Customer));
                newNode->next = customerList;
                customerList = newNode;
                
                insertIntoTree(&customerTree, cust.id, (void*)newNode);
            }
        }
        fclose(file);
    }
}

void initializeTrees() {
    carVinTree = NULL;
    salesPersonTree = NULL;
    customerTree = NULL;
    carSalesTree = NULL;
    
    // Showroom-specific trees will be initialized in loadDataFromFiles
}

void addCar(Car* car) {
    // Generate a new VIN if not provided
    if (strlen(car->VIN) == 0) {
        char* vin = createNewId("CAR");
        strcpy(car->VIN, vin);
        free(vin);
    }
    
    // Create a new car node
    CarNode* newNode = (CarNode*)malloc(sizeof(CarNode));
    if (!newNode) {
        fprintf(stderr, "Memory allocation failed\n");
        return;
    }
    
    // Copy the car data
    memcpy(&newNode->car, car, sizeof(Car));
    
    // Insert into linked list
    newNode->next = carList;
    carList = newNode;
    
    // Insert into main B+ tree
    insertIntoTree(&carVinTree, car->VIN, (void*)newNode);
    
    // Insert into showroom-specific tree
    for (int i = 0; i < numShowrooms; i++) {
        if (strcmp(showrooms[i].id, car->showroomId) == 0) {
            insertIntoTree(&showroomCarTrees[i], car->VIN, (void*)newNode);
            break;
        }
    }
    
    // Save to file
    saveCarToFile(car);
    
    printf("Car added with VIN: %s\n", car->VIN);
}

void addCustomer(Customer* customer) {
    // Generate a new ID if not provided
    if (strlen(customer->id) == 0) {
        char* id = createNewId("CUST");
        strcpy(customer->id, id);
        free(id);
    }
    
    // Create a new customer node
    CustomerNode* newNode = (CustomerNode*)malloc(sizeof(CustomerNode));
    if (!newNode) {
        fprintf(stderr, "Memory allocation failed\n");
        return;
    }
    
    // Copy the customer data
    memcpy(&newNode->customer, customer, sizeof(Customer));
    
    // Insert into linked list
    newNode->next = customerList;
    customerList = newNode;
    
    // Insert into B+ tree
    insertIntoTree(&customerTree, customer->id, (void*)newNode);
    
    // Save to file
    saveCustomerToFile(customer);
    
    printf("Customer added with ID: %s\n", customer->id);
}

int main() {
    // Initialize file system
    ensureFilesExist();
    
    // Initialize data structures
    initializeTrees();
    
    // Load existing data
    loadDataFromFiles();
    
    int choice;
    char VIN[MAX_STRING];
    char customerId[MAX_STRING];
    char salesPersonId[MAX_STRING];
    char paymentType[MAX_STRING];
    int emiMonths;
    double downPayment;
    double minSales, maxSales;
    int minMonths, maxMonths;
    char outputFileName[MAX_STRING];
    
    do {
        printf("\n===== Car Dealership Management System =====\n");
        printf("1. Add a new car\n");
        printf("2. Add a new salesperson\n");
        printf("3. Add a new customer\n");
        printf("4. Sell a car\n");
        printf("5. Display car information\n");
        printf("6. Find most popular car\n");
        printf("7. Find most successful salesperson\n");
        printf("8. Find salespersons by target range\n");
        printf("9. List customers by EMI range\n");
        printf("10. Predict next month sales\n");
        printf("11. Merge showroom data to file\n");
        printf("12. Exit\n");
        printf("Enter your choice: ");
        scanf("%d", &choice);
        getchar();  // Consume newline
        
        switch (choice) {
            case 1: {
                Car newCar;
                memset(&newCar, 0, sizeof(Car));
                newCar.available = true;
                
                printf("Enter car details:\n");
                printf("VIN (leave empty for auto-generation): ");
                fgets(newCar.VIN, MAX_STRING, stdin);
                newCar.VIN[strcspn(newCar.VIN, "\r\n")] = 0;
                
                printf("Name: ");
                fgets(newCar.name, MAX_STRING, stdin);
                newCar.name[strcspn(newCar.name, "\r\n")] = 0;
                
                printf("Color: ");
                fgets(newCar.color, MAX_STRING, stdin);
                newCar.color[strcspn(newCar.color, "\r\n")] = 0;
                
                printf("Price: ");
                scanf("%lf", &newCar.price);
                getchar();  // Consume newline
                
                printf("Fuel Type: ");
                fgets(newCar.fuelType, MAX_STRING, stdin);
                newCar.fuelType[strcspn(newCar.fuelType, "\r\n")] = 0;
                
                printf("Body Type (Hatchback/Sedan/SUV): ");
                fgets(newCar.bodyType, MAX_STRING, stdin);
                newCar.bodyType[strcspn(newCar.bodyType, "\r\n")] = 0;
                
                printf("Showroom ID: ");
                fgets(newCar.showroomId, MAX_STRING, stdin);
                newCar.showroomId[strcspn(newCar.showroomId, "\r\n")] = 0;
                
                addCar(&newCar);
                break;
            }
            case 2: {
                SalesPerson newSP;
                memset(&newSP, 0, sizeof(SalesPerson));
                
                printf("Enter salesperson details:\n");
                printf("ID (leave empty for auto-generation): ");
                fgets(newSP.id, MAX_STRING, stdin);
                newSP.id[strcspn(newSP.id, "\r\n")] = 0;
                
                printf("Name: ");
                fgets(newSP.name, MAX_STRING, stdin);
                newSP.name[strcspn(newSP.name, "\r\n")] = 0;
                
                printf("Showroom ID: ");
                fgets(newSP.showroomId, MAX_STRING, stdin);
                newSP.showroomId[strcspn(newSP.showroomId, "\r\n")] = 0;
                
                printf("Target (in lakhs): ");
                scanf("%lf", &newSP.target);
                
                addNewSalesPerson(&newSP);
                break;
            }
            case 3: {
                Customer newCustomer;
                memset(&newCustomer, 0, sizeof(Customer));
                
                printf("Enter customer details:\n");
                printf("ID (leave empty for auto-generation): ");
                fgets(newCustomer.id, MAX_STRING, stdin);
                newCustomer.id[strcspn(newCustomer.id, "\r\n")] = 0;
                
                printf("Name: ");
                fgets(newCustomer.name, MAX_STRING, stdin);
                newCustomer.name[strcspn(newCustomer.name, "\r\n")] = 0;
                
                printf("Mobile Number: ");
                fgets(newCustomer.mobileNo, MAX_STRING, stdin);
                newCustomer.mobileNo[strcspn(newCustomer.mobileNo, "\r\n")] = 0;
                
                printf("Address: ");
                fgets(newCustomer.address, MAX_STRING, stdin);
                newCustomer.address[strcspn(newCustomer.address, "\r\n")] = 0;
                
                addCustomer(&newCustomer);
                break;
            }
            case 4:
                printf("Enter VIN of car to sell: ");
                fgets(VIN, MAX_STRING, stdin);
                VIN[strcspn(VIN, "\r\n")] = 0;
                
                printf("Enter customer ID: ");
                fgets(customerId, MAX_STRING, stdin);
                customerId[strcspn(customerId, "\r\n")] = 0;
                
                printf("Enter salesperson ID: ");
                fgets(salesPersonId, MAX_STRING, stdin);
                salesPersonId[strcspn(salesPersonId, "\r\n")] = 0;
                
                printf("Payment Type (Cash/Loan): ");
                fgets(paymentType, MAX_STRING, stdin);
                paymentType[strcspn(paymentType, "\r\n")] = 0;
                
                if (strcmp(paymentType, "Loan") == 0) {
                    printf("EMI Months: ");
                    scanf("%d", &emiMonths);
                    
                    printf("Down Payment: ");
                    scanf("%lf", &downPayment);
                } else {
                    emiMonths = 0;
                    downPayment = 0;
                }
                
                sellCarToCustomer(VIN, customerId, salesPersonId, paymentType, emiMonths, downPayment);
                break;
            case 5:
                printf("Enter VIN of car to display: ");
                fgets(VIN, MAX_STRING, stdin);
                VIN[strcspn(VIN, "\r\n")] = 0;
                
                displayCarInfo(VIN);
                break;
            case 6: {
                char* popularCar = findMostPopularCar();
                printf("Most popular car: %s\n", popularCar);
                free(popularCar);
                break;
            }
            case 7: {
                SalesPerson* bestSP = findMostSuccessfulSalesPerson();
                if (bestSP) {
                    printf("Most successful salesperson: %s (%.2f lakhs)\n", 
                           bestSP->name, bestSP->achieved);
                    printf("Incentive: %.2f lakhs\n", bestSP->achieved * 0.01); // Print the incentive
                } else {
                    printf("No salespeople found in the system\n");
                }
                break;
            }
            case 8:
                printf("Enter minimum sales (in lakhs): ");
                scanf("%lf", &minSales);
                
                printf("Enter maximum sales (in lakhs): ");
                scanf("%lf", &maxSales);
                
                findSalesPersonByTargetRange(minSales, maxSales);
                break;
            case 9:
            // Call the function with the specific range for EMI
            listCustomersByEmiRange(36, 48);
            break;
            case 10:
                predictNextMonthSales();
                break;
            case 11:
                printf("Enter output file name: ");
                fgets(outputFileName, MAX_STRING, stdin);
                outputFileName[strcspn(outputFileName, "\r\n")] = 0;
                
                mergeShowrooms(outputFileName);
                break;
            case 12:
                printf("Exiting...\n");
                break;
            default:
                printf("Invalid choice. Please try again.\n");
        }
    } while (choice != 12);
    
    freeMemory();
    return 0;
}