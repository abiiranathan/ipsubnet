let calc;

function calculateSubnet(e) {
  e?.preventDefault();
  if (!calc) {
    return;
  }

  const subnetInput = document.getElementById("subnet-input");
  const subnetCountInput = document.getElementById("subnet-count");
  if (subnetInput.value.trim() == "" && subnetCountInput.value.trim() == "") {
    alert("Enter Network ID and number of subnets required");
    return;
  }

  // Create a subnet from a string
  var subnetString = subnetInput.value.trim();
  var subnetCount = parseInt(subnetCountInput.value.trim());

  let subnet;
  try {
    subnet = calc.create_subnet(subnetString);
    document.getElementById("subnetInfo").innerText = subnetString;
  } catch (error) {
    alert("Invalid IP Address. Make sure CIDR is in range /24 to /32");
    return;
  }

  // Classify IP
  let ipClass = calc.classify_ip(subnet.ip);
  document.getElementById("ipClass").innerText = String.fromCharCode(ipClass);

  // Compute assignable addresses
  let assignableAddresses = calc.compute_assignable_addresses(subnetString);

  document.getElementById("assignableAddresses").innerText =
    assignableAddresses;

  // Get subnet table information
  let subnetTable = calc.get_subnet_table(subnetString, subnetCount);

  // Display subnet table in table
  var subnetTableBody = document.getElementById("subnetTable");
  subnetTableBody.innerHTML = "";
  for (var i = 0; i < subnetTable.size(); i++) {
    var row = subnetTableBody.insertRow();
    var info = subnetTable.get(i);

    row.insertCell().innerText = info.network_id;
    row.insertCell().innerText = info.subnet_mask;
    row.insertCell().innerText =
      info.host_range_start + " - " + info.host_range_end;
    row.insertCell().innerText = info.num_usable_hosts;
    row.insertCell().innerText = info.broadcast_id;
  }
}

Module.onRuntimeInitialized = function () {
  // Create a SubnetCalculator instance
  calc = new Module.SubnetCalculator();
  console.log("WebAssemply is ready!");

  const form = document.querySelector("form");
  form.onsubmit = calculateSubnet;

  // initial call
  calculateSubnet();
};
