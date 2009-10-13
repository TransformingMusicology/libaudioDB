function plotLSHresults(filename, w, kk, useLog)
if nargin<4, useLog=0;end
if nargin<3, kk=1;end

R = load(filename,'ascii'); % Assume 10 radii queries x 10 radii result summaries

for k=1:10
  RR(k,:)=mean(R(k:10:end,:))./100; % Convert to Probability
end

c=['r' 'g' 'b' 'c' 'm' 'y' 'k'];

if(useLog)
     logSym=' Log_{10} ';
   else
     logSym='';
end

figure
subplot(211)
hold on
radii=[0.1 0.2 0.4 0.5 0.7 0.9 1 2 5 10];
for k=1:10
  if(useLog)
    semilogx(radii, kk*log10(lshP(w,radii./radii(k))),[num2str(c(mod(k,length(c))+1)) '-+'],'lineWidth',2)
  else
    plot(radii,lshP(w,radii./radii(k)).^kk,[num2str(c(mod(k,length(c))+1)) '-+'],'lineWidth',2)
  end
end
if(useLog)
  axis([radii(1) radii(end) -kk 0])
else
  axis([0 10 0 1])
end
grid on
legend([{"r=.1"},{"r=.2"},{"r=.4"},{"r=.5"},{"r=.7"},{"r=.9"},{"r=1"},{"r=2"},{"r=5"},{"r=10"}])
title(['Estimated Probability for 10 LSH radii searches: w=' num2str(w) ' , k=' num2str(kk)])
xlabel('Radius')
ylabel([logSym 'Probability'])


subplot(212)
hold on
for k=1:10
  if(useLog)
    semilogx(radii,kk*log10(RR(k,:)),[num2str(c(mod(k,length(c))+1)) '-+'],'lineWidth',2)
  else
    plot(radii,RR(k,:).^kk,[num2str(c(mod(k,length(c))+1)) '-+'],'lineWidth',2)    
  end
end
if(useLog)
  axis([radii(1) radii(end) -kk 0])
else
  axis([0 10 0 1])
endif
grid on
legend([{"r=.1"},{"r=.2"},{"r=.4"},{"r=.5"},{"r=.7"},{"r=.9"},{"r=1"},{"r=2"},{"r=5"},{"r=10"}])
title(['Observed Probability for 10 LSH radii searches: w=' num2str(w) ' , k=' num2str(kk)])
xlabel('Radius')
ylabel([logSym 'Probability'])

