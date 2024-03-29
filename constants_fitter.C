// Stand alone code to fit for alignment parameters A,e,M from mass fits 

#include "Minuit2/FunctionMinimum.h"
#include "Minuit2/MnMinimize.h"
#include "Minuit2/MnMigrad.h"
#include "Minuit2/MnPrint.h"
#include "Minuit2/MnUserParameterState.h"
#include "Minuit2/FCNGradientBase.h"

#include <sys/time.h>

using namespace ROOT::Minuit2;
using namespace std;

class TheoryFcn : public FCNGradientBase {

public:

  TheoryFcn(const vector<double> &meas, const vector<double> &err, const vector<string> &bin_labels) : scaleSquared(meas), scaleSquaredError(err), binLabels(bin_labels), errorDef(1.0) {}
  ~TheoryFcn() {}

  virtual double Up() const {return errorDef;}
  virtual void SetErrorDef(double def) {errorDef = def;}

  virtual double operator()(const vector<double>&) const;
  virtual vector<double> Gradient(const vector<double>& ) const;
  virtual bool CheckGradient() const {return true;} // TODO true means it compares analytic grad to numerical, but what does it do based on the answer? 
  // virtual std::vector< double > ROOT::Minuit2::FCNGradientBase::Hessian(const std::vector< double > & )const -> allows to do Hessian analytically? 

  vector<int> getIndices(string bin_label) const; 
  double getK(const int pT_index) const;
  
  const vector<double> pT_binning {25.0, 33.3584, 38.4562, 42.2942, 45.9469, 55.0}; // pT binning goes here
  static constexpr double scaling_A = 0.001, scaling_e = 0.001 * 40.0, scaling_M = 0.001 / 40.0; // this scaling makes fitter parameters of order 1 

  vector<string> binLabels; // made public for the closure test
  static const int n_eta_bins = 24; //TODO automatise, so don't input by hand!!!

private:

  vector<double> scaleSquared;
  vector<double> scaleSquaredError;
  double errorDef;
};

//-----------------------------------------------
class TheoryFcn2 : public TheoryFcn {

public:
  
  TheoryFcn2(const vector<double> &meas, const vector<double> &err, const vector<string> &bin_labels) : TheoryFcn(meas, err, bin_labels) {}
  ~TheoryFcn2() {}

  vector<vector<double>> DummyData(const vector<double> &dummy_par_val, const int n_data_points, const double width, TRandom3 random) const; //TODO  vector<vector<double>> might be slow
  
};

//-----------------------------------------------
// Function to get bin index from label name

vector<int> TheoryFcn::getIndices(string bin_label) const{
  vector<int> indices;
  int pos = 0;
  for (int i=0; i<4-1;i++){ //4 for 4D binning
    pos = bin_label.find("_");
    indices.push_back(stoi(bin_label.substr(0,pos)));
    bin_label.erase(0,pos+1); // 1 is the length of the delimiter, "_"
  }
  indices.push_back(stoi(bin_label));
  return indices;
}

//-----------------------------------------------
// Function to get average k from pT bin index

double TheoryFcn::getK(const int pT_index) const{
  // TODO what about errors on k?
  // TODO could add the pT histo per eta,pt,eta,pt to get more accurate average pT
  return 2.0 / (pT_binning[pT_index] + pT_binning[pT_index+1]); // k = 1 / avg_pT
}

//-----------------------------------------------
// Function to build reduced chi2 

double TheoryFcn::operator()(const vector<double>& par) const {
  // par has size n_parameters = 3*number_eta_bins, idices from 0 to n-1 contain A, from n to 2n-1 epsilon, from 2n to 3n-1 M
  assert(par.size() == 3*n_eta_bins); 

  double chi2(0.0);
  double diff(0.0);

  double k_plus, k_minus, term_pos, term_neg;  
  double my_func;
  int eta_pos_index, eta_neg_index;
  vector<int> bin_indices(4); // for 4D binning

  for(unsigned int n(0); n < scaleSquared.size() ; n++) {
    bin_indices = getIndices(binLabels[n]); //TODO I hope overwriting this vector is ok
    eta_pos_index = bin_indices[0];
    eta_neg_index = bin_indices[2];

    // get k value and TODO error from pT bin index
    k_plus = getK(bin_indices[1]); 
    k_minus = getK(bin_indices[3]);

    // (1 + A(+) - e(+)k + M(+)/k)(1 + A(-) - e(-)k - M(-)/k) and scaling of e,M
    term_pos = (1. + scaling_A*par[eta_pos_index] - scaling_e*par[eta_pos_index + n_eta_bins]*k_plus +  scaling_M*par[eta_pos_index + 2*n_eta_bins]/k_plus);
    term_neg = (1. + scaling_A*par[eta_neg_index] - scaling_e*par[eta_neg_index + n_eta_bins]*k_minus - scaling_M*par[eta_neg_index + 2*n_eta_bins]/k_minus);
    
    my_func = term_pos*term_neg; 
    
    diff = my_func - scaleSquared[n]; 
    chi2 += diff*diff/(scaleSquaredError[n]*scaleSquaredError[n]);

  }
 
  int ndof = scaleSquared.size() - par.size();
 
  return chi2/ndof; // minimise reduced chi2 directly 

}

//-----------------------------------------------
// Function to build gradient of reduced chi2 analytically 

vector<double> TheoryFcn::Gradient(const vector<double> &par ) const {

  assert(par.size() == 3*n_eta_bins);

  vector<double> grad(par.size(),0.0);

  double temp(0.0), local_func(0.0), local_grad(0.0);

  double k_plus, k_minus, term_pos, term_neg;

  int eta_pos_index, eta_neg_index;
  vector<int> bin_indices(4); // for 4D binning
  
  int ndof = scaleSquared.size() - par.size();

  for(unsigned int n(0); n < scaleSquared.size() ; n++) { // loops over measurements
    // TODO I need everything from the chi2 function, maybe better solution to this
    bin_indices = getIndices(binLabels[n]); // TODO is overwriting a vector like this safe?
    eta_pos_index = bin_indices[0];
    eta_neg_index = bin_indices[2];

    // get k value and TODO error from pT bin index
    k_plus = getK(bin_indices[1]); 
    k_minus = getK(bin_indices[3]);

    // (1 + A(+) - e(+)k + M(+)/k)(1 + A(-) - e(-)k - M(-)/k) and scaling of e,M
    term_pos = (1. + scaling_A*par[eta_pos_index] - scaling_e*par[eta_pos_index + n_eta_bins]*k_plus +  scaling_M*par[eta_pos_index + 2*n_eta_bins]/k_plus);
    term_neg = (1. + scaling_A*par[eta_neg_index] - scaling_e*par[eta_neg_index + n_eta_bins]*k_minus - scaling_M*par[eta_neg_index + 2*n_eta_bins]/k_minus);
    
    local_func = term_pos*term_neg;
    
    temp=2.0*(local_func - scaleSquared[n])/(scaleSquaredError[n]*scaleSquaredError[n])/ndof; 
    
    //TODO not calculate terms before checking if needed, ahm, not needed
    for (unsigned int i : {eta_pos_index, eta_pos_index + n_eta_bins, eta_pos_index + 2*n_eta_bins, eta_neg_index, eta_neg_index + n_eta_bins, eta_neg_index + 2*n_eta_bins}) { // loops over parameters
      if (i == eta_pos_index) { // derivative wrt A(+), which is fpar[eta_pos_index] 
	local_grad = scaling_A*term_neg;
      } else if (i == eta_pos_index + n_eta_bins) { // derivative wrt e(+)
      	local_grad = -scaling_e*k_plus*term_neg;
      } else if (i == eta_pos_index + 2*n_eta_bins) { // derivative wrt M(+)
	local_grad = scaling_M/k_plus*term_neg;
      } else if (i == eta_neg_index){ // derivative wrt A(-)
	local_grad = scaling_A*term_pos;
      } else if (i == eta_neg_index + n_eta_bins) { // derivative wrt e(-)
	local_grad = -scaling_e*k_minus*term_pos;
      } else if (i == eta_neg_index + 2*n_eta_bins){ // derivative wrt M(-)
	local_grad = -scaling_M/k_minus*term_pos;
      } else {
	cout<<"\n"<<"ERROR: indices in grad don't match"<<"\n"; //TOOD raise a proper error
      }
      
      grad[i] += temp*local_grad; 
    }
  }
  return grad;
}

//-----------------------------------------------
// Function to generate dummy data for closure test

vector<vector<double>> TheoryFcn2::DummyData(const vector<double> &dummy_par_val, const int n_data_points, const double width, TRandom3 random) const {
  // par has size n_parameters = 3*number_eta_bins, idices from 0 to n-1 contain A, from n to 2n-1 epsilon, from 2n to 3n-1 M
  
  vector<vector<double>> res(2);
  vector<double> test_data(n_data_points, 0.0), test_error(n_data_points, 0.0);
  double k_plus, k_minus, mean, width_rand, term_pos, term_neg;
  int eta_pos_index, eta_neg_index;
  vector<double> DummyParVal(dummy_par_val);

  vector<int> bin_indices(4); // for 4D binning
  
  for(unsigned int n(0); n <n_data_points ; n++) {
    bin_indices = getIndices(binLabels[n]); //TODO I hope overwriting this vector is ok
    eta_pos_index = bin_indices[0];
    eta_neg_index = bin_indices[2];

    // get k value and TODO error from pT bin index
    k_plus = getK(bin_indices[1]);
    k_minus = getK(bin_indices[3]);
  
    // (1 + A(+) - e(+)k + M(+)/k)(1 + A(-) - e(-)k - M(-)/k) and scaling of e,M
    term_pos = (1. + DummyParVal[eta_pos_index] - DummyParVal[eta_pos_index + n_eta_bins]*k_plus +  DummyParVal[eta_pos_index + 2*n_eta_bins]/k_plus);
    term_neg = (1. + DummyParVal[eta_neg_index] - DummyParVal[eta_neg_index + n_eta_bins]*k_minus - DummyParVal[eta_neg_index + 2*n_eta_bins]/k_minus);

    mean = term_pos*term_neg;
    width_rand = random.Gaus(width, width/10.0);
    test_error[n] = width_rand;
    test_data[n] = random.Gaus(mean, width_rand);
    
  }

  res[0] = test_data;
  res[1] = test_error;

  return res;
}

//-----------------------------------------------
// Function to get parameter name (A0, e1, etc.) and appropriate scaling from index in parameters array

tuple<string,double> getParameterNameAndScaling(int index){

  int whole = index / TheoryFcn::n_eta_bins;
  int rest = index % TheoryFcn::n_eta_bins;
  double scaling;

  string name;
  if (whole == 0) {
    name = "A" + to_string(rest);
    scaling = TheoryFcn::scaling_A;
  }
  else if (whole == 1) {
    name = "e" + to_string(rest);
    scaling = TheoryFcn::scaling_e;
  }
  else if (whole == 2) {
    name = "M" + to_string(rest);
    scaling = TheoryFcn::scaling_M;
  }
  else {
    cout<<"\n"<<"ERROR counting parameters"<<"\n";
  }

  return make_tuple(name, scaling);
}

//-----------------------------------------------
// Function to generate dummy labels for closure test

vector<string> generateLabels (const int n_eta_bins, const int n_pt_bins){
  
  vector<string> labels(n_eta_bins*n_eta_bins*n_pt_bins*n_pt_bins);
  int m = 0; 

  for (int i=0; i<n_eta_bins; i++){
    for (int j=0; j<n_pt_bins; j++){
      for (int k=0; k<n_eta_bins; k++){
	for (int l=0; l<n_pt_bins; l++){
	  labels[m] = to_string(i) + "_" + to_string(j) + "_" + to_string(k) + "_" + to_string(l);
	  m ++;
	}
      }
    }
  }
  
  return labels;
}

//-----------------------------------------------
// Function to generate dummy parameters for closure test

vector<double> generateParameters (const int n_parameters){ //TODO might be slow to pass the vector by value

  vector<double> res(n_parameters); // has size n_parameters = 3*number_eta_bins, idices from 0 to n-1 contain A, from n to 2n-1 epsilon, from 2n to 3n-1 M 
  for (int i=0; i<n_parameters/3; i++){ // A from -0.0002 to 0.0005 and back
    res[i] = (-7.0*36.0/n_parameters/n_parameters*(i - n_parameters/6)*(i - n_parameters/6) + 5.0)*0.0001;
    //res[i] = 0.0; // A set to 0 for debugging
  }
  for (int i=n_parameters/3; i<2*n_parameters/3; i++){ // e from 0.01 to 0.001 and back
    //res[i] = (9.0*36.0/n_parameters/n_parameters*((i-n_parameters/3) - n_parameters/6)*((i-n_parameters/3) - n_parameters/6) + 1.0)*0.001;  
    res[i] = 0.0; // e set to 0 for debugging
  }
  for (int i=2*n_parameters/3; i<n_parameters; i++){ // M from 4*10^-5 to -2*10^-5 and back
    res[i] = ( 6.0*36.0/n_parameters/n_parameters*((i-2*n_parameters/3) - n_parameters/6)*((i-2*n_parameters/3) - n_parameters/6) - 2.0)*0.00001;
    //res[i] = 0.0; // M set to 0 for debugging 
  }
  return res;
}

//-----------------------------------------------
// Overload << operator

template <typename S>
ostream& operator<<(ostream& os,
                    const vector<S>& vector)
{
  for (auto element : vector) {
    os << element << " ";
  }
  return os;
}

//-----------------------------------------------

int constants_fitter() {
  
  double t1(0.);
  // Get start time
  struct timeval tv_start, tv_stop;
  gettimeofday(&tv_start, 0);

  int n_tries = 1;

  auto track_tree = std::make_unique<TTree>("track_tree", "track_tree");

  double red_chi2, edm;
  double* red_chi2_ptr = &red_chi2;
  double* edm_ptr = &edm;

  int n_eta_bins_tmp = 24; // TODO must match n_eta_bins in for loop
  vector<double> params_tmp(3*n_eta_bins_tmp); // 3 for A,e,M model

  track_tree->Branch("red_chi2", red_chi2_ptr);
  track_tree->Branch("edm", edm_ptr);
  for(int w = 0; w<3*n_eta_bins_tmp; w++){ // 3 for A,e,M model
    int rest_tmp = w % n_eta_bins_tmp;
    if( w / n_eta_bins_tmp == 0) track_tree->Branch(Form("pull_A%d",rest_tmp), &params_tmp[w]);
    //if( w / n_eta_bins_tmp == 1) track_tree->Branch(Form("e%d",w), &params_tmp[w]);
    if( w / n_eta_bins_tmp == 2) track_tree->Branch(Form("pull_M%d",rest_tmp), &params_tmp[w]);
  }
  
  int z=0;
  //for(int z = 0; z<n_tries; z++){
  TRandom3 iter_random = TRandom3(4357 + z); //TRandom3(4357 + z) was before when i ran 1000 toys, now i am running toys on top of those 1000 
  int verbosity = 0; 
  ROOT::Minuit2::MnPrint::SetGlobalLevel(verbosity);
  
  // Choose closure_test/analysis mode
  string mode_option("closure_test"); //TODO pass as command line argument

  int n_eta_bins = 0;
  unsigned int n_data_points, n_parameters;
  vector<string> labels;
  vector<double> dummy_parameters(0.0);
  vector<double> scale_squared_values, scale_squared_error_values;

  if (mode_option.compare("closure_test") == 0) {

    //-----------------------------------------------
    // Get dummy data for closure test

    n_eta_bins = 24;
    int n_pt_bins = 5; 
    labels = generateLabels(n_eta_bins,n_pt_bins);
    
    n_data_points = labels.size(); 
    n_parameters = 3*n_eta_bins; // 3 for A,e,M model  
    
    vector<double> empty_data(n_data_points, 0.0), empty_error(n_data_points, 0.0); 
    double error_start = 0.001;
    dummy_parameters = generateParameters(n_parameters);
    
    TheoryFcn2 f_dummy(empty_data, empty_error, labels);
    vector<vector<double>> dummy_call = f_dummy.DummyData(dummy_parameters, n_data_points, error_start, iter_random);
    scale_squared_values = dummy_call[0];
    scale_squared_error_values = dummy_call[1];
    
  } else if (mode_option.compare("analysis") == 0){
    
    //-----------------------------------------------
    // Get data
    
    std::unique_ptr<TFile> inputFile( TFile::Open("mass_fits_control_histos_smear_beta_val.root") );
    std::unique_ptr<TH1D> scale(inputFile->Get<TH1D>("epsilon")); //TODO change to beta
    
    n_data_points = 12; // TODO scale->GetEntries();
    
    for(int i=0; i<n_data_points; i++){
      scale_squared_values[i] = (scale->GetBinContent(i+1))*(scale->GetBinContent(i+1)); // TODO change that we fit for beta in mass
      scale_squared_error_values[i] = 2*abs(scale->GetBinContent(i+1))*(scale->GetBinError(i+1));
      labels[i] = scale->GetXaxis()->GetLabels()->At(i)->GetName();
      std::cout << "\n" << labels[i] << " scale_squared_values " << scale_squared_values[i] << " scale_squared_error_values " << scale_squared_error_values[i] << "\n";
    }
    
    n_eta_bins = 24; //TODO work out from labels variable counter script
    n_parameters = 3*n_eta_bins; // 3 for A,e,M model
  
  }

  //-------------------------------------------------
  // Use data

  TheoryFcn fFCN(scale_squared_values,scale_squared_error_values,labels);
  fFCN.SetErrorDef(1.0/(n_data_points - n_parameters)); // new error definition when minimising reduced chi2

  // Create parameters with initial starting values

  double start=0.0, par_error=0.001; // Will store error on parameter before taking scaling into account
  MnUserParameters upar;
  //cout << "Precision: " << upar.Precision() << "\n";
  //upar.SetPrecision(8.881784197e-16);

  //  for(unsigned int i=0 ; i<n_parameters; ++i){
  //  upar.Add(Form("param%d",i), start, par_error);
  //}

  for (int i=0; i<n_parameters/3; i++){ // A 
    upar.Add(Form("param%d",i), start, par_error); //TODO in closure mode can use dummy_parameters[i] for start value
  }
  for (int i=n_parameters/3; i<2*n_parameters/3; i++){ // e
    upar.Add(Form("param%d",i), start); // all e fixed to 0.0
  }
  for (int i=2*n_parameters/3; i<n_parameters; i++){ // M 
    upar.Add(Form("param%d",i), start, par_error); 
  }

  //for (unsigned int i=0; i<upar.Params().size(); i++) {
  //cout <<"par[" << i << "] = " << get<0>(getParameterNameAndScaling(i)) << ": " << upar.Params()[i] << "\n";
  //}

  // create Migrad minimizer

  MnMigrad minimize(fFCN, upar, 1); //TODO strategy is 1, check others too

  // ... and Minimize

  unsigned int maxfcn(numeric_limits<unsigned int>::max());
  double tolerance(0.001); //MIGRAD will stop iterating when edm : 0.002 * tolerance * UPERROR
  //fFCN.SetErrorDef(1.0/(n_data_points - n_parameters)); 
  //3829.158
  
  //double t1(0.);
  // Get start time
  //struct timeval tv_start, tv_stop;
  //gettimeofday(&tv_start, 0);

  FunctionMinimum min = minimize(maxfcn, tolerance);
  
  red_chi2 = min.Fval();
  edm = min.Edm();
  for (int i=0; i<n_parameters; i++){
    if(i / n_eta_bins == 0 || i / n_eta_bins == 2) params_tmp[i] = (min.UserState().Value(i) * get<1>(getParameterNameAndScaling(i)) - dummy_parameters[i]) / (min.UserState().Error(i) * abs(get<1>(getParameterNameAndScaling(i)))); // NOTE fit only A,M for now
  } 

  track_tree->Fill();

  //cout<<"iter "<<z<<": "<<red_chi2<<" ; edm: "<< edm <<"\n";

  //} //comment this bracket for loop over n_tries

  std::unique_ptr<TFile> f_control_iter( TFile::Open("track_params.root", "RECREATE") );
  track_tree->Write();
  cout<<"\n"<<"done"<<"\n";
  f_control_iter->Close();

  gettimeofday(&tv_stop, 0);
  t1 = (tv_stop.tv_sec - tv_start.tv_sec)*1000.0 + (tv_stop.tv_usec - tv_start.tv_usec)/1000.0;
  
  cout << "# Calculation time: " << t1 << " ms" << "\n";

  
  cout << "chi^2/ndf: " << min.Fval() << "\n" << "\n";

  cout << "min is valid: " << min.IsValid() << std::endl;
  cout << "HesseFailed: " << min.HesseFailed() << std::endl;
  cout << "HasCovariance: " << min.HasCovariance() << std::endl;
  cout << "HasValidCovariance: " << min.HasValidCovariance() << std::endl;
  cout << "HasValidParameters: " << min.HasValidParameters() << std::endl;
  cout << "IsAboveMaxEdm: " << min.IsAboveMaxEdm() << std::endl;
  cout << "HasReachedCallLimit: " << min.HasReachedCallLimit() << std::endl;
  cout << "HasAccurateCovar: " << min.HasAccurateCovar() << std::endl;
  cout << "HasPosDefCovar : " << min.HasPosDefCovar() << std::endl;
  cout << "HasMadePosDefCovar : " << min.HasMadePosDefCovar() << std::endl;

  cout << min << "\n";
  
  vector<double> hessian = min.UserState().Hessian().Data();
  vector<double> covariance = min.UserState().Covariance().Data();
  
  TH2D *hessian_hist = new TH2D("hessian_hist", "Hessian", n_eta_bins*2, -7000.0, 7000.0, n_eta_bins*2, -7000.0, 7000.0); //TODO change s*2
  TH2D *covariance_hist = new TH2D("covariance_hist", "Covariance", n_eta_bins*2, -7000.0, 7000.0, n_eta_bins*2, -7000.0, 7000.0); //TODO change s*2

  int bin;
  for (int i=1; i<=n_eta_bins*2; i++){ //TODO change s*2
    for (int j=1; j<=i; j++){
      bin = hessian_hist->GetBin(i, n_eta_bins*2+1-j); //TODO change s*2
      hessian_hist->SetBinContent(bin, hessian[(i-1)*(i)/2+(j-1)]); 
      covariance_hist->SetBinContent(bin, covariance[(i-1)*(i)/2+(j-1)]); 
      bin = hessian_hist->GetBin(j, n_eta_bins*2+1-i); //TODO change s*2
      hessian_hist->SetBinContent(bin, hessian[(i-1)*(i)/2+(j-1)]); 
      covariance_hist->SetBinContent(bin, covariance[(i-1)*(i)/2+(j-1)]); 
    }
  }
  /*
  double hessian_ar[n_eta_bins*2*n_eta_bins*2]; //TODO change s*2
  double covariance_ar[n_eta_bins*2*n_eta_bins*2]; //TODO change s*2

  for (int i=1; i<=n_eta_bins*2; i++){ //TODO change s*2
    for (int j=1; j<=n_eta_bins*2; j++){ //TODO change s*2
      // define arrays collumn wise
      hessian_ar[(i-1)*n_eta_bins*2+(j-1)] = hessian_hist->GetBinContent(hessian_hist->GetBin(i, n_eta_bins*2+1-j)); //TODO change s*2
      covariance_ar[(i-1)*n_eta_bins*2+(j-1)] = covariance_hist->GetBinContent(covariance_hist->GetBin(i, n_eta_bins*2+1-j)); //TODO change s*2
    }
  }
  
  cout << "Hessian: " << hessian <<"\n"<< "Hessian: " <<"\n";
  
  TMatrixTSym<double> Hessian_matrix(n_eta_bins*2, hessian_ar, "F"); //need option F to unroll collumn wise //TODO change s*2, pass by ptr
  
  for(int i=0; i<n_eta_bins*2; i++){ //TODO change s*2
    for(int j=0; j<n_eta_bins*2; j++){ //TODO change s*2
      cout << Hessian_matrix(i,j) << " ";
    }
    cout << "\n";
  }
  cout << "\n";
  cout << "Covariance: " << covariance << "\n" << "Covariance: " << "\n";
 
  TMatrixTSym<double> Covariance_matrix(n_eta_bins*2, covariance_ar, "F"); //need option F to unroll collumn wise //TODO change s*2, pass by ptr

  for(int i=0; i<n_eta_bins*2; i++){ //TODO change s*2
    for(int j=0; j<n_eta_bins*2; j++){ //TODO change s*2
      cout << Covariance_matrix(i,j) << " ";
    }
    cout << "\n";
  }
  */
  unique_ptr<TFile> f_a( TFile::Open("a.root", "RECREATE") );

  TCanvas *c4 = new TCanvas("c4","c4",800,600);
  hessian_hist->SetStats(0);
  hessian_hist->Draw("COLZ");
  f_a->WriteObject(c4, "hessian_hist");

  TCanvas *c5 = new TCanvas("c5","c5",800,600);
  covariance_hist->SetStats(0);
  covariance_hist->Draw("COLZ");
  f_a->WriteObject(c5, "covariance_hist");

  /*
  cout << "\n" << "Fitted A [ ], e [GeV], M [GeV^-1] parameters: " << "\n";
  for (unsigned int i(0); i<upar.Params().size(); i++) {
    cout <<"par[" << i << "]: " << get<0>(getParameterNameAndScaling(i)) << " fitted to: "<< min.UserState().Value(i) * get<1>(getParameterNameAndScaling(i)) << " +/- " << min.UserState().Error(i) * abs(get<1>(getParameterNameAndScaling(i))) << "\n";
  }
  */  

  // Histogram with dummy parameters and fitted parameters

  TH1D *dummy_pars_A = new TH1D("dummy_pars_A", "A", n_eta_bins, -2.4, 2.4);
  TH1D *dummy_pars_e = new TH1D("dummy_pars_e", "e", n_eta_bins, -2.4, 2.4);
  TH1D *dummy_pars_M = new TH1D("dummy_pars_M", "M", n_eta_bins, -2.4, 2.4);
  
  TH1D *fitted_pars_A = new TH1D("fitted_pars_A", "A", n_eta_bins, -2.4, 2.4);
  TH1D *fitted_pars_e = new TH1D("fitted_pars_e", "e", n_eta_bins, -2.4, 2.4);
  TH1D *fitted_pars_M = new TH1D("fitted_pars_M", "M", n_eta_bins, -2.4, 2.4);

  for (int i=1; i<=n_eta_bins; i++){
    fitted_pars_A->SetBinContent(i, min.UserState().Value(i-1) * get<1>(getParameterNameAndScaling(i-1)));
    fitted_pars_A->SetBinError(i, min.UserState().Error(i-1) * abs(get<1>(getParameterNameAndScaling(i-1))));

    fitted_pars_e->SetBinContent(i, min.UserState().Value(i-1+n_eta_bins) * get<1>(getParameterNameAndScaling(i-1+n_eta_bins)));
    fitted_pars_e->SetBinError(i, min.UserState().Error(i-1+n_eta_bins) * abs(get<1>(getParameterNameAndScaling(i-1+n_eta_bins))));
    
    fitted_pars_M->SetBinContent(i, min.UserState().Value(i-1+2*n_eta_bins) * get<1>(getParameterNameAndScaling(i-1+2*n_eta_bins)));
    fitted_pars_M->SetBinError(i, min.UserState().Error(i-1+2*n_eta_bins) * abs(get<1>(getParameterNameAndScaling(i-1+2*n_eta_bins))));

    if (mode_option.compare("closure_test") == 0) { 
      dummy_pars_A->SetBinContent(i, dummy_parameters[i-1]);
      dummy_pars_A->SetBinError(i, 1e-10);
      dummy_pars_e->SetBinContent(i, dummy_parameters[i-1+n_eta_bins]);
      dummy_pars_e->SetBinError(i, 1e-10);
      dummy_pars_M->SetBinContent(i, dummy_parameters[i-1+2*n_eta_bins]);
      dummy_pars_M->SetBinError(i, 1e-10);
    }
}

  unique_ptr<TFile> f_control( TFile::Open("constants_fitted.root", "RECREATE") ); 

  TCanvas *c1 = new TCanvas("c1","c1",800,600);

  auto leg1 = new TLegend(0.68, 0.78, 0.90, 0.90);
    
  leg1->SetFillStyle(0);
  leg1->SetBorderSize(0);
  leg1->SetTextSize(0.025);
  leg1->SetFillColor(10);
  leg1->SetNColumns(1);
  leg1->SetHeader("");
  
  fitted_pars_A->SetMinimum(-0.00025);
  fitted_pars_A->SetMaximum(0.0007);
  fitted_pars_A->GetYaxis()->SetNoExponent();
  fitted_pars_A->SetStats(0);
  fitted_pars_A->GetXaxis()->SetTitle("#eta");
  fitted_pars_A->GetYaxis()->SetTitle("Magnetic field correction");
  fitted_pars_A->Draw("");
  
  if (mode_option.compare("closure_test") == 0) {
    dummy_pars_A->SetLineColor(kRed);
    dummy_pars_A->SetMarkerStyle(kPlus);
    dummy_pars_A->SetMarkerColor(kRed);
    dummy_pars_A->Draw("SAME");
    
    leg1->AddEntry(dummy_pars_A, "input par", "l");
  }
  
  leg1->AddEntry(fitted_pars_A, "fitted par", "l");
  leg1->Draw("SAME");
  leg1->Clear();

  TCanvas *c2 = new TCanvas("c2","c2",800,600);

  fitted_pars_e->SetMinimum(-0.001);
  fitted_pars_e->SetMaximum(0.017);
  fitted_pars_e->GetYaxis()->SetNoExponent();
  fitted_pars_e->SetStats(0);
  fitted_pars_e->GetXaxis()->SetTitle("#eta");
  fitted_pars_e->GetYaxis()->SetTitle("Material correction (GeV)");
  fitted_pars_e->Draw("");

  if (mode_option.compare("closure_test") == 0) {
    dummy_pars_e->SetLineColor(kRed);
    dummy_pars_e->SetMarkerStyle(kPlus);
    dummy_pars_e->SetMarkerColor(kRed);
    dummy_pars_e->Draw("SAME");

    leg1->AddEntry(dummy_pars_e, "input par", "l");
  }

  leg1->AddEntry(fitted_pars_e, "fitted par", "l");
  leg1->Draw("SAME");
  leg1->Clear();
  
  TCanvas *c3 = new TCanvas("c3","c3",800,600);

  fitted_pars_M->SetMinimum(-3e-5);
  fitted_pars_M->SetMaximum(4.5e-5);
  fitted_pars_M->SetStats(0);
  fitted_pars_M->GetXaxis()->SetTitle("#eta");
  fitted_pars_M->GetYaxis()->SetTitle("Misalignment correction (GeV^{-1})");
  fitted_pars_M->Draw("");

  if (mode_option.compare("closure_test") == 0) {
    dummy_pars_M->SetLineColor(kRed);
    dummy_pars_M->SetMarkerStyle(kPlus);
    dummy_pars_M->SetMarkerColor(kRed);
    dummy_pars_M->Draw("SAME");

    leg1->AddEntry(dummy_pars_M, "input par", "l");
  }

  leg1->AddEntry(fitted_pars_M, "fitted par", "l");
  leg1->Draw("SAME");
  
  f_control->WriteObject(c1, "A");
  f_control->WriteObject(c2, "e");
  f_control->WriteObject(c3, "M");
 
  return 0;
}


