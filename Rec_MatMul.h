#include <vector>
#include <algorithm>
#include "cache_size.h"
#include <cmath>
const int BLOCK_SIZE =  24; //can use which does not consider cache organization sqrt((CacheDetector::getL1CacheSize()*1024)/12);


namespace MatMath {

    struct Mat {
        int rows, cols;
        std::vector<int> matrix;
        Mat(int r, int c) : rows(r), cols(c), matrix(r * c, 0) {}
    };

    Mat BlockedTPIt(Mat& m1){
        Mat temp(m1.cols, m1.rows);
        for(int i = 0; i < m1.rows; i += BLOCK_SIZE){
            for(int j = 0; j < m1.cols; j += BLOCK_SIZE){
                for(int ii = i; ii < std::min(i + BLOCK_SIZE,m1.rows);++ii){
                    for(int jj = j; jj < std::min(j + BLOCK_SIZE,m1.cols);++jj){
                        temp.matrix[jj * m1.rows + ii] = m1.matrix[ii* m1.cols + jj];

                    }
                }
            } 

        }

       return temp;
    }
    // Standard multiplication with direct access
    void MultiplyMat(Mat& result, const Mat& mat1, const Mat& mat2,
                     int r1_start, int r1_end, int c1_start, int c1_end,
                     int r2_start, int r2_end, int c2_start, int c2_end,
                     int r_res_start, int c_res_start) {
        int r_size = r1_end - r1_start;
        int c_size = c2_end - c2_start;
        for (int i = 0; i < r_size && r_res_start + i < result.rows; i++) {
            for (int j = 0; j < c_size && c_res_start + j < result.cols; j++) {
                int sum = 0;
                for (int k = c1_start; k < c1_end; k++) {
                    sum += mat1.matrix[(r1_start + i) * mat1.cols + k] * 
                           mat2.matrix[k * mat2.cols + (c2_start + j)];
                }
                result.matrix[(r_res_start + i) * result.cols + (c_res_start + j)] = sum;
            }
        }
    }


    // Add matrices in-place with direct access
    void add(Mat& result, const Mat& mat1, const Mat& mat2,
             int r_start, int r_end, int c_start, int c_end,
             int r_res_start, int c_res_start) {
        int r_size = r_end - r_start;
        int c_size = c_end - c_start;
        for (int i = 0; i < r_size && r_res_start + i < result.rows; i++) {
            for (int j = 0; j < c_size && c_res_start + j < result.cols; j++) {
                result.matrix[(r_res_start + i) * result.cols + (c_res_start + j)] = 
                    mat1.matrix[(r_start + i) * mat1.cols + (c_start + j)] + 
                    mat2.matrix[(r_start + i) * mat2.cols + (c_start + j)];
            }
        }
    }

    // Recursive multiplication without copying
    void matMul(Mat& result, const Mat& mat1, const Mat& mat2,
                int r1_start, int r1_end, int c1_start, int c1_end,
                int r2_start, int r2_end, int c2_start, int c2_end,
                int r_res_start, int c_res_start) {
        int r1_size = r1_end - r1_start;
        int c1_size = c1_end - c1_start; // Also mat2 rows
        int c2_size = c2_end - c2_start;

        // Clamp to matrix bounds
        r1_end = std::min(r1_end, mat1.rows);
        c1_end = std::min(c1_end, mat1.cols);
        r2_end = std::min(r2_end, mat2.rows);
        c2_end = std::min(c2_end, mat2.cols);

        // Base case
        if (r1_size <= 64 || c1_size <= 64 || c2_size <= 64 || 
            r1_size <= 0 || c1_size <= 0 || c2_size <= 0) {
            MultiplyMat(result, mat1, mat2,
                        r1_start, r1_end, c1_start, c1_end,
                        r2_start, r2_end, c2_start, c2_end,
                        r_res_start, c_res_start);
            return;
        }

        int mid1 = r1_start + r1_size / 2;
        int mid2 = c1_start + c1_size / 2; // Also r2_size / 2
        int mid3 = c2_start + c2_size / 2;

        // Adjust mids to not exceed bounds
        mid1 = std::min(mid1, mat1.rows);
        mid2 = std::min(mid2, mat1.cols);
        mid3 = std::min(mid3, mat2.cols);

        // Temporary buffers sized to submatrix dimensions
        int temp_r_size = mid1 - r1_start; // Top-left rows
        int temp_c_size = mid3 - c2_start; // Top-left cols
        Mat temp1(temp_r_size, temp_c_size);
        Mat temp2(temp_r_size, temp_c_size);

        // Top-left: C11 = A11*B11 + A12*B21
        matMul(temp1, mat1, mat2, r1_start, mid1, c1_start, mid2, r2_start, mid2, c2_start, mid3, 0, 0);
        matMul(temp2, mat1, mat2, r1_start, mid1, mid2, c1_end, mid2, r2_end, c2_start, mid3, 0, 0);
        add(result, temp1, temp2, 0, temp_r_size, 0, temp_c_size, r_res_start, c_res_start);

        // Top-right: C12 = A11*B12 + A12*B22
        int temp_c_size_right = c2_end - mid3;
        temp1 = Mat(temp_r_size, temp_c_size_right);
        temp2 = Mat(temp_r_size, temp_c_size_right);
        matMul(temp1, mat1, mat2, r1_start, mid1, c1_start, mid2, r2_start, mid2, mid3, c2_end, 0, 0);
        matMul(temp2, mat1, mat2, r1_start, mid1, mid2, c1_end, mid2, r2_end, mid3, c2_end, 0, 0);
        add(result, temp1, temp2, 0, temp_r_size, 0, temp_c_size_right, r_res_start, c_res_start + temp_c_size);

        // Bottom-left: C21 = A21*B11 + A22*B21
        int temp_r_size_bottom = r1_end - mid1;
        temp1 = Mat(temp_r_size_bottom, temp_c_size);
        temp2 = Mat(temp_r_size_bottom, temp_c_size);
        matMul(temp1, mat1, mat2, mid1, r1_end, c1_start, mid2, r2_start, mid2, c2_start, mid3, 0, 0);
        matMul(temp2, mat1, mat2, mid1, r1_end, mid2, c1_end, mid2, r2_end, c2_start, mid3, 0, 0);
        add(result, temp1, temp2, 0, temp_r_size_bottom, 0, temp_c_size, r_res_start + temp_r_size, c_res_start);

        // Bottom-right: C22 = A21*B12 + A22*B22
        temp1 = Mat(temp_r_size_bottom, temp_c_size_right);
        temp2 = Mat(temp_r_size_bottom, temp_c_size_right);
        matMul(temp1, mat1, mat2, mid1, r1_end, c1_start, mid2, r2_start, mid2, mid3, c2_end, 0, 0);
        matMul(temp2, mat1, mat2, mid1, r1_end, mid2, c1_end, mid2, r2_end, mid3, c2_end, 0, 0);
        add(result, temp1, temp2, 0, temp_r_size_bottom, 0, temp_c_size_right, r_res_start + temp_r_size, c_res_start + temp_c_size);
    }

    // Wrapper
    Mat matMul(const Mat& mat1, const Mat& mat2) {
        Mat result(mat1.rows, mat2.cols);
        matMul(result, mat1, mat2, 0, mat1.rows, 0, mat1.cols, 0, mat2.rows, 0, mat2.cols, 0, 0);
        return result;
    }

    Mat BlockedMul(const Mat& mat1, const Mat& mat2){
        Mat result(mat1.rows, mat2.cols);
        
        for (int i = 0; i < mat1.rows; i += BLOCK_SIZE) {
            for (int j = 0; j < mat2.cols; j += BLOCK_SIZE) {
                for (int k = 0; k < mat1.cols; k += BLOCK_SIZE) {
                    for (int ii = i; ii < std::min(i + BLOCK_SIZE, mat1.rows); ii++) {
                        for (int jj = j; jj < std::min(j + BLOCK_SIZE, mat2.cols); jj++) {
                            int sum = 0;
                            for (int kk = k; kk < std::min(k + BLOCK_SIZE, mat1.cols); kk++) {
                                sum += mat1.matrix[ii * mat1.cols + kk] * mat2.matrix[kk * mat2.cols + jj];
                            }
                            result.matrix[ii * result.cols + jj] += sum;
                        }
                    }
                }
            }
        }
        return result;
    }
}

