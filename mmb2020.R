
rm(list = ls())

data <- read.csv(file="mmb2020_linkAll_modeAll_tranGiaClient.csv", header=TRUE, sep=",")



#
# print statistics (websites are identical for all configurations)
#
if(FALSE)
{
for (idxLink in c('dsl1Mbps15ms_sat', 'dsl_sat20Mbps300ms', 'dsl1Mbps15ms_sat20Mbps300ms', 'dsl20Mbps15ms_sat'))
{
  for (idxMode in c('s', 'p', 'h'))
  {
    print(sprintf("Link %s, mode %s", idxLink, idxMode));
    print(sprintf("nrMainObjects mean: %f", mean(data[which(data$link==idxLink & data$mode==idxMode), ]$nrMainObjects)))
    print(sprintf("nrEmbObjects mean: %f",  mean(data[which(data$link==idxLink & data$mode==idxMode), ]$nrEmbObjects)))
    
    sizeMainObjs <- c()
    sizeEmbObjs <- c()
    for(line in 1:nrow(data[which(data$link==idxLink & data$mode==idxMode), ]))
    {
      # add every rows"43:3242:3242:324:" "456:544:456:" "78:9898:898:" to one big list  
      sizeMainObjOneWebsite <- as.numeric(strsplit(as.character(data[which(data$link==idxLink & data$mode==idxMode), ]$mainObjectSizes[line]), ":")[[1]])
      sizeMainObjs <- c(sizeMainObjs, sizeMainObjOneWebsite)    
      sizeEmbObjOneWebsite <- as.numeric(strsplit(as.character(data[which(data$link==idxLink & data$mode==idxMode), ]$embObjectSizes[line]), ":")[[1]])
      sizeEmbObjs <- c(sizeEmbObjs, sizeEmbObjOneWebsite)
    }
    print(sprintf("sizeMainObjects mean: %f", mean(sizeMainObjs)))
    print(sprintf("sizeEmbObjects mean: %f", mean(sizeEmbObjs)))
  }
  print(' ')
}
} #if(TRUE/FALSE)



#
# print website size distribution 
#
cairo_pdf(filename="mmb2020_websiteSize.pdf", width = 5, height = 5)
par(mar = c(3, 4, 2.5, 2))
totalWebsiteSize <- (  data[which(data$link=='dsl1Mbps15ms_sat' & data$mode=='s'), ]$mainObjectsTotalSize
                     + data[which(data$link=='dsl1Mbps15ms_sat' & data$mode=='s'), ]$embObjectsTotalSize)/1000 #in kbyte
plot(0,0, xlim=c(0,4000), ylim=c(0,1.0), type="n", xlab="total websize size [kbyte]", ylab="CDF", mgp=c(2,1,0))
lines(ecdf(totalWebsiteSize), pch="")
dev.off()



#
# print simulation results
#
data$duration <- (data$finishTime-data$startTime)/1000000000

cairo_pdf(filename="mmb2020_sequential.pdf", width = 5, height = 5)
par(mar = c(3, 4, 2.5, 2)) #bottom,left,top,right
plot(0,0, xlim=c(0, 20), ylim=c(0,1.0), type="n", xlab="page load time [s]", ylab="CDF", mgp=c(2,1,0))
lines(ecdf(data[which(data$link=='dsl1Mbps15ms_sat'            & data$mode=='s'), ]$duration), col="green", pch="")
lines(ecdf(data[which(data$link=='dsl_sat20Mbps300ms'          & data$mode=='s'), ]$duration), col="blue", pch="")
lines(ecdf(data[which(data$link=='dsl1Mbps15ms_sat20Mbps300ms' & data$mode=='s'), ]$duration), col="red", pch="")
lines(ecdf(data[which(data$link=='dsl20Mbps15ms_sat'           & data$mode=='s'), ]$duration), col="purple", pch="")
title("Sequential HTTP request(s)/response(s)")
legend('bottomright', legend=c("Terrestrial 1Mbit/s 15ms",
                               "Satellite 20Mbit/s 300ms",
                               "TMC Multipath",
                               "Terrestrial 20Mbit/s 15ms"),
       col=c("green", "blue", "red", "purple"),pch=15)
dev.off()

cairo_pdf(filename="mmb2020_parallel.pdf", width = 5, height = 5)
par(mar = c(3, 4, 2.5, 2))
#par(mar = c(3, 4, 0.5, 2))
plot(0,0, xlim=c(0, 20), ylim=c(0,1.0), type="n", xlab="page load time [s]", ylab="CDF", mgp=c(2,1,0))
lines(ecdf(data[which(data$link=='dsl1Mbps15ms_sat'            & data$mode=='p'), ]$duration), col="green", pch="")
lines(ecdf(data[which(data$link=='dsl_sat20Mbps300ms'          & data$mode=='p'), ]$duration), col="blue", pch="")
lines(ecdf(data[which(data$link=='dsl1Mbps15ms_sat20Mbps300ms' & data$mode=='p'), ]$duration), col="red", pch="")
lines(ecdf(data[which(data$link=='dsl20Mbps15ms_sat'           & data$mode=='p'), ]$duration), col="purple", pch="")
title("Parallel HTTP request(s)/response(s) (HTTP/1.1)")
legend('bottomright', legend=c("Terrestrial 1Mbit/s 15ms",
                               "Satellite 20Mbit/s 300ms",
                               "TMC Multipath",
                               "Terrestrial 20Mbit/s 15ms"),
       col=c("green", "blue", "red", "purple"),pch=15)
dev.off()

cairo_pdf(filename="mmb2020_http2.pdf", width = 5, height = 5)
par(mar = c(3, 4, 2.5, 2))
plot(0,0, xlim=c(0, 20), ylim=c(0,1.0), type="n", xlab="page load time [s]", ylab="CDF", mgp=c(2,1,0))
lines(ecdf(data[which(data$link=='dsl1Mbps15ms_sat'            & data$mode=='h'), ]$duration), col="green", pch="")
lines(ecdf(data[which(data$link=='dsl_sat20Mbps300ms'          & data$mode=='h'), ]$duration), col="blue", pch="")
lines(ecdf(data[which(data$link=='dsl1Mbps15ms_sat20Mbps300ms' & data$mode=='h'), ]$duration), col="red", pch="")
lines(ecdf(data[which(data$link=='dsl20Mbps15ms_sat'           & data$mode=='h'), ]$duration), col="purple", pch="")
title("Single HTTP request/response (HTTP/2)")
legend('bottomright', legend=c("Terrestrial 1Mbit/s 15ms",
                               "Satellite 20Mbit/s 300ms",
                               "TMC Multipath",
                               "Terrestrial 20Mbit/s 15ms"),
       col=c("green", "blue", "red", "purple"),pch=15)
dev.off()



#
# Data sent via TMC PEPs from web server to client
# 
sizeDist=read.csv(file="mmb2020_linkAll_modeAll_tmcPepRight.csv", header=TRUE, sep=",")

for (idxLink in c('dsl1Mbps15ms_sat', 'dsl_sat20Mbps300ms', 'dsl1Mbps15ms_sat20Mbps300ms', 'dsl20Mbps15ms_sat'))
{
  for (idxMode in c('s', 'p', 'h'))
  {
    sumSizeTer   <- sum(as.numeric(sizeDist[which(sizeDist$link==idxLink & sizeDist$mode==idxMode), ]$sizeTer))
    sumSizeSat   <- sum(as.numeric(sizeDist[which(sizeDist$link==idxLink & sizeDist$mode==idxMode), ]$sizeSat))
    sumSizeTotal <- sum(as.numeric(sizeDist[which(sizeDist$link==idxLink & sizeDist$mode==idxMode), ]$sizeTotal))
    print(sprintf("link %s, mode %s, sizeTotal %f Mbyte, ter %f%%   sat %f%%", idxLink, idxMode, sumSizeTotal/1000000, sumSizeTer/sumSizeTotal, sumSizeSat/sumSizeTotal))
  }
  print(' ')
}


