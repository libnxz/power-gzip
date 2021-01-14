#!/bin/bash
# Generate test scripts.

file=$(echo ${1} | cut -d . -f1)
level=$(echo ${1} | cut -d . -f2)
action=$(echo ${1} | cut -d . -f3)
type=$(echo ${1} | cut -d . -f4)

. config.sh

case ${action} in
  "compress") test="${LIBNXZ} -c | ${GZIP} -cd -";;
  "decompress") test="${GZIP} -${level} -c - | ${LIBNXZ} -c -d";;
  "compdecomp") test="${LIBNXZ} -c | ${LIBNXZ} -c -d";;
esac

cat - >${1} <<EOF
#!/bin/bash

set -eo pipefail

checksum1=\$(${AWK} '{print \$1}' ${file}.checksum)

if [[ -z "\${checksum1}" ]]; then
  echo "Error: The checksum of the original file is null."
  exit 1
fi

echo -e "Original checksum:\t\${checksum1}"

checksum2=\$(cat ${file}.uncompressed | \\
	    ${test} \\
	    | ${SHA256SUM} - \\
	    | ${AWK} '{print \$1}')

if [[ -z "\${checksum2}" ]]; then
  echo "Error: The checksum of the generated file is null."
  exit 1
fi

echo -e  "Final checksum:\t\${checksum1}"

if [[ "\${checksum1}" != "\${checksum2}" ]]; then
  echo "Error: checksum did not match"
  exit 1
fi
EOF

chmod +x ${1}
