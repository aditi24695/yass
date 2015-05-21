#!/bin/bash

#set -x
thres=1.5

source_file_dir=... # directory where unstemmed lexicon is kept
                    # NOTE: lexicon must be split into several parts, each 
                    # corresponding to a single letter of the alphabet, and
                    # containing all words starting with that letter, e.g.
                    # a.txt, b.txt, etc. resp. for English.
output_file_dir=... # directory where output will go

for i in a b c d e f g h i j k l m n o p q r s t u v w x y z ; do
    wc=`cat $source_file_dir/${i}.txt | wc -l`
    cluster-lexicon ${source_file_dir}/${i}.txt $wc ${output_file_dir}/${i}_stem_${thres}.txt ${output_file_dir}/${i}_f_${thres}.txt ${thres}
    echo "file ${i}.txt complete"
done

cat ${output_file_dir}/*stem_${thres}* > ${output_file_dir}/eng-${thres}.txt
export  LC_ALL="C"
sort ${output_file_dir}/eng-${thres}.txt > ${output_file_dir}/eng-${thres}.sort.txt
export  LC_ALL="en_US.UTF-8"
echo "Sort complete"
