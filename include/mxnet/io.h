/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

/*!
 *  Copyright (c) 2015 by Contributors
 * \file io.h
 * \brief mxnet io data structure and data iterator
 */
#ifndef MXNET_IO_H_
#define MXNET_IO_H_

#include <vector>
#include <string>
#include <utility>
#include <queue>
#include "dmlc/data.h"
#include "dmlc/registry.h"
#include "./base.h"
#include "./ndarray.h"

namespace mxnet {
/*!
 * \brief iterator type
 * \tparam DType data type
 */
template<typename DType>
class IIterator : public dmlc::DataIter<DType> {
 public:
  /*!
   * \brief set the parameters and init iter
   * \param kwargs key-value pairs
   */
  virtual void Init(const std::vector<std::pair<std::string, std::string> >& kwargs) = 0;
  /*! \brief reset the iterator */
  virtual void BeforeFirst(void) = 0;
  /*! \brief move to next item */
  virtual bool Next(void) = 0;
  /*! \brief get current data */
  virtual const DType &Value(void) const = 0;
  /*! \brief constructor */
  virtual ~IIterator(void) {}
  /*! \brief store the name of each data, it could be used for making NDArrays */
  std::vector<std::string> data_names;
  /*! \brief set data name to each attribute of data */
  inline void SetDataName(const std::string data_name) {
    data_names.push_back(data_name);
  }
  /*! \brief request iterator length hint for current epoch. 
   * Note that the returned value can be < 0, indicating
   * that the length of iterator is unknown unless you went through all data. 
   */
  virtual int64_t GetLenHint(void) const {
    return -1;
  }
};  // class IIterator

/*! \brief a single data instance */
struct DataInst {
  /*! \brief unique id for instance */
  unsigned index;
  /*! \brief content of data */
  std::vector<TBlob> data;
  /*! \brief extra data to be fed to the network */
  std::string extra_data;
};  // struct DataInst

/*!
 * \brief DataBatch of NDArray, returned by Iterator
 */
struct DataBatch {
  /*! \brief content of dense data, if this DataBatch is dense */
  std::vector<NDArray> data;
  /*! \brief index of image data */
  std::vector<uint64_t> index;
  /*! \brief extra data to be fed to the network */
  std::string extra_data;
  /*! \brief num of example padded to batch */
  int num_batch_padd;
};  // struct DataBatch

/*! \brief typedef the factory function of data iterator */
typedef std::function<IIterator<DataBatch> *()> DataIteratorFactory;
/*!
 * \brief Registry entry for DataIterator factory functions.
 */
struct DataIteratorReg
    : public dmlc::FunctionRegEntryBase<DataIteratorReg,
                                        DataIteratorFactory> {
};
//--------------------------------------------------------------
// The following part are API Registration of Iterators
//--------------------------------------------------------------
/*!
 * \brief Macro to register Iterators
 *
 * \code
 * // example of registering a mnist iterator
 * REGISTER_IO_ITER(MNISTIter)
 * .describe("Mnist data iterator")
 * .set_body([]() {
 *     return new PrefetcherIter(new MNISTIter());
 *   });
 * \endcode
 */
#define MXNET_REGISTER_IO_ITER(name)                                    \
  DMLC_REGISTRY_REGISTER(::mxnet::DataIteratorReg, DataIteratorReg, name)

/*!
 * \brief A random accessable dataset which provides GetLen() and GetItem().
 * Unlike DataIter, it's a static lookup storage which is friendly to random access.
 * The dataset itself should NOT contain data processing, which should be applied during
 * data augmentation or transformation processes.
 */
class Dataset {
  public:
    /*!
    *  \brief Initialize the Operator by setting the parameters
    *  This function need to be called before all other functions.
    *  \param kwargs the keyword arguments parameters
    */
    virtual void Init(const std::vector<std::pair<std::string, std::string> >& kwargs) = 0;
    /*! 
    *  \brief Get the size of the dataset
    */
    virtual uint64_t GetLen(void) const = 0;
    /*! 
    *  \brief Get the output size. 
    *  For example, if the GetItem should return two array, the output size is 2.
    */
    virtual int GetOutputSize(void) const = 0;
    /*! 
    *  \brief Get the ndarray data given index in dataset and output tuple
    *  \param idx the integer index for required data
    *  \param n the n-th item
    */
    virtual NDArray GetItem(uint64_t idx, int n, int* is_scalar) const = 0;
    // virtual destructor
    virtual ~Dataset(void) {}
    /*!
    * \brief factory function
    * \param name Name of the dataset
    * \return The created dataset.
    */
    static Dataset* Create(const std::string& name);
};  // class Dataset

/*! \brief typedef the factory function of dataset */
typedef std::function<Dataset *()> DatasetFactory;
/*!
 * \brief Registry entry for Dataset factory functions.
 */
struct DatasetReg
    : public dmlc::FunctionRegEntryBase<DatasetReg,
                                        DatasetFactory> {
};
//--------------------------------------------------------------
// The following part are API Registration of Datasets
//--------------------------------------------------------------
/*!
 * \brief Macro to register Datasets
 *
 * \code
 * // example of registering an image sequence dataset
 * REGISTER_IO_ITE(ImageSequenceDataset)
 * .describe("image sequence dataset")
 * .set_body([]() {
 *     return new ImageSequenceDataset();
 *   });
 * \endcode
 */
#define MXNET_REGISTER_IO_DATASET(name)                                    \
  DMLC_REGISTRY_REGISTER(::mxnet::DatasetReg, DatasetReg, name)

class BatchifyFunction {
  public:
    /*! \brief Destructor */
    virtual ~BatchifyFunction(void) {};
    /*! \brief Init */
    virtual void Init(const std::vector<std::pair<std::string, std::string> >& kwargs) = 0;
    /*! \brief The batchify logic */
    virtual std::vector<NDArray> Batchify(std::vector<std::vector<NDArray> >& inputs) = 0;
  protected:
    std::size_t SanityCheck(std::vector<std::vector<NDArray> >& inputs) {
      auto bs = inputs.size();
      CHECK_GT(bs, 0) << "BatchifyFunction should handle at lease 1 sample";
      auto out_size = inputs[0].size();
      // sanity check: each input has same size
      for (size_t i = 1; i < bs; ++i) {
          CHECK_EQ(inputs[i].size(), out_size)
            << i << "-th input size does not match " << out_size;
      }
      return out_size;
    }
};  // class BatchifyFunction

/*! \brief typedef the factory function of data sampler */
typedef std::function<BatchifyFunction *()> BatchifyFunctionFactory;
/*!
 * \brief Registry entry for DataSampler factory functions.
 */
struct BatchifyFunctionReg
    : public dmlc::FunctionRegEntryBase<BatchifyFunctionReg,
                                        BatchifyFunctionFactory> {
};
//--------------------------------------------------------------
// The following part are API Registration of Batchify Function
//--------------------------------------------------------------
/*!
 * \brief Macro to register Batchify Functions
 *
 * \code
 * // example of registering a Batchify Function
 * MXNET_REGISTER_IO_BATCHIFY_FUNCTION(StackBatchify)
 * .describe("Stack Batchify Function")
 * .set_body([]() {
 *     return new StackBatchify();
 *   });
 * \endcode
 */
#define MXNET_REGISTER_IO_BATCHIFY_FUNCTION(name)                                    \
  DMLC_REGISTRY_REGISTER(::mxnet::BatchifyFunctionReg, BatchifyFunctionReg, name)
}  // namespace mxnet
#endif  // MXNET_IO_H_
