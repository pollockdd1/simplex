model.set_name("GNR")

state_names = config.get_string_array("MODEL.states")

states.set(state_names)

for k, v in pairs(state_names) do
   print(k, v)
end

param_template = {initial_value = 0.001, step_size = 0.00005}

print(#state_names)

Q = {}

for i=1,#state_names do
	Q[i] = {}
	for j=1,#state_names do
	   Q[i][j] = Parameter.new(tostring(state_names[i])..tostring(state_names[j]), "continuous", param_template)
	end
	model.add_rate_vector(RateVector.new("RV-"..tostring(state_names[i]), {state = states[i], pos = {}}, Q[i]))
end
