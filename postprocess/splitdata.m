function split = splitdata(data, len)
% PRE: data has 2 dimensions, where length of first dimension is a multiple
% of len
% Split data into an 3-dimensional array, where split is performed
% by splitting first dimension of data into len-length chunks

  if (ndims(data) ~= 2)
    fprintf('Error: data must have 2 dimensions\n');
    return
  elseif (mod(length(data(:,1)), len) ~= 0)
    fprintf('Error: length of first dimension of data must be a multiple of len\n');
    return
  else
    n = length(data(:,1))/len;
    for i = 1:n
        start = 1 + (i-1)*len;
        fin = i*len;
        split(i,:,:)=data(start:fin,:);
    end
  end
end
