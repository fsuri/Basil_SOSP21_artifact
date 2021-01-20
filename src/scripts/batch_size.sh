if [ "$#" -le 0 ]
then

  echo "Usage: batch_size.sh {batch size}"

else


ls config | while read shard
do
    echo modifying ${shard} for batch size $1
    sed -i "1c\block-size = ${1}" ./config/${shard}/hotstuff.gen.conf
done

fi
