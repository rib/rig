#!/bin/sh 

TEST_PATH=$1
TEST_OUT=${TEST_PATH}/ppm

mkdir -p ${TEST_OUT}

gifdecode()
{
    OUTF=$(basename ${1} .gif)
    ${TEST_PATH}/test_decode_gif ${1} > ${TEST_OUT}/${OUTF}.ppm
}


for GIF in $(ls test/data/*.gif);do
    gifdecode ${GIF}
done

